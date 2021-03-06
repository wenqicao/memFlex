#include "mem_swap.h"
#include "mapper.h"
#include "bitmap.h"
#include <linux/delay.h>
#include <linux/swapfile.h>
#include <linux/kmod.h>

#define PROACTIVE_SWAP
#define TH 0.95

extern void __iomem * Nahanni_mem;
extern spinlock_t bitmap_lock;
extern int meta_size;
extern void switch_to_next_swap(void);

int entry_size = 4096 + sizeof(struct mdata);

static struct swap_info_struct *gsis;

static int swap_switched = 0;
static int init = 0;
spinlock_t init_lock;

unsigned long shm_total = (1<<21); /*# of pages*/
unsigned long memswap_total = 0; /*# of pages*/
unsigned long memswap_chunk = (1<<20); /*# of pages*/

unsigned long chunk_num;

struct task_struct *swapin_thread;
int mempipe_swapin_thread(void *data);
int swap_mounted = 0;

/*
* each memswap area consists of one or more chunks
* each chunk size is @memswap_size
*/
int memswap_init(struct swap_info_struct *sis){
	
	int ret = 0;
	unsigned long chunk_offset;/*In terms of memswap_chunks*/
	char *p = (char *)Nahanni_mem + meta_size;
	
	if(Nahanni_mem == NULL){
		printk(KERN_ERR "Nahanni_mem = NULL\n");
		return -1;
	}
	
	/*For now, a shared memory swap partition consists of at most 8 sections*/
	sis->mapper = init_mapper(8);

	if(sis->mapper == NULL){
		printk(KERN_ERR "sis->mapper initialization fails\n");
		return -1;
	}

	chunk_num = shm_total/memswap_chunk;
	
	spin_lock(&bitmap_lock);
	chunk_offset = first_zero_bit(p, chunk_num);
        printk("init chunk_offset = %ld\n", chunk_offset);

        if(chunk_offset != -1){
                setbit(p, chunk_offset);
		memswap_total += memswap_chunk;
        }else{
		printk("ERROR: invalid chunk offset -1");
		spin_unlock(&bitmap_lock);
		return -1;
	}
	spin_unlock(&bitmap_lock);

	insert_mapper(&sis->mapper, chunk_offset);

	sis->shm = p + entry_size;
	sis->shm_start = sis->shm_end = (unsigned long)(sis->shm + chunk_offset*memswap_chunk*entry_size);
        sis->disk_start = sis->disk_end = 0;

	gsis = sis;
	
	swapin_thread = NULL;
	//swapin_thread = kthread_run(mempipe_swapin_thread, NULL, "mempipe_swapin");
	if(swapin_thread == NULL){
		printk("MemFlex error: create swapin thread failed\n");
	}

	return ret;
}

struct swapin_mdata* get_swapin_mdata(unsigned long offset)
{
	struct mdata *m = (struct mdata *)((char *)gsis->shm + offset*entry_size + PAGE_SIZE);
	if(m->exist == '0') {
		return NULL;
	}else{
		return &m->sm;
	}

}


int mempipe_swapin_thread(void *data){
	struct sysinfo i;
	int err;
        char path[256] = "/sbin/swapoff";
        char *argv[] = { path, "/dev/vda5", NULL };
       	char *envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };


	while(!kthread_should_stop()){
		if(kthread_should_stop()){
			break;	
		}
				
		msleep(1000);
		si_meminfo(&i);
		
		if(swap_mounted == 1 && i.freeram > (gsis->disk_end - gsis->disk_start)){	
			err = call_usermodehelper(path, argv, envp, 1);
			if (err = 0) {
				gsis->disk_end = gsis->disk_start = 0;
				swap_mounted = 0;
			}else{
				printk("MemFlex error: call_usermodehelper\n");
			}
		}
	}

	return 0;
}

int add_memswap_chunk(void){
	char *p = (char *)Nahanni_mem + meta_size;
	unsigned long chunk_offset;

	spin_lock(&bitmap_lock);
	chunk_offset = first_zero_bit(p, chunk_num);
	setbit(p, chunk_offset);
	printk("add chunk, offset = %ld\n", chunk_offset);
	spin_unlock(&bitmap_lock);
	
	/*If the shared memory is full, switch to disk swap*/
	if(chunk_offset == -1)
		return -1;
	
	insert_mapper(&gsis->mapper, chunk_offset);
	memswap_total += memswap_chunk;	
	return 0;
}

int almost_full(void){
	if((gsis->disk_end - gsis->disk_start) > (memswap_total*TH))
		return 1;
	else 
		return 0;
}

int mempipe_swap_writepage(struct page *page, struct writeback_control *wbc,
void (*end_write_func)(struct bio *, int))
{
        int ret = 0;
        struct swap_info_struct *sis = page_swap_info(page);
	pgoff_t offset;
	unsigned long shm_offset;
        swp_entry_t entry;
	struct mdata *m;

        if (sis->flags & SWP_FILE) {
        	printk("Error: writepage to a FILE\n");
	}

        entry.val = page_private(page);
        offset = swp_offset(entry);

	if(offset == 0){
		printk("write page, offset = %ld, shm_start = %ld\n", offset, sis->shm_start);
        }
	if(init == 0){
		spin_lock(&init_lock);
		if(init == 0){
			printk("start memswap_init()\n");
			ret = memswap_init(sis); 
			init = 1;
		}
		spin_unlock(&init_lock);
	}

	BUG_ON(sis->mapper == NULL);
	
	
	/*The swap slot is in shared memory*/
	count_vm_event(PSWPOUT);

	m = (struct mdata *)((char *)sis->shm + offset*entry_size + PAGE_SIZE);
        m->exist = '0';

#ifdef PROACTIVE_SWAP
        if(page->idx == 1) {
                pgd_t *pgd;
                pud_t *pud;
                pmd_t *pmd;

		
                unsigned long address = page->rmap_addrs[0];
                struct vm_area_struct *vma = page->rmap_vmas[0];

		if(vma == NULL)
			goto skip;

                pgd = pgd_offset(vma->vm_mm, address);
                if(pgd == NULL)
			goto skip;

		pud = pud_offset(pgd, address);
		if(pud == NULL)
			goto skip;

                pmd = pmd_offset(pud, address);
		if(pmd == NULL)
			goto skip;
	
               	m->sm.vma = vma;
                m->sm.pmd = pmd;
                m->sm.address = address;

		m->exist = '1';
        }
skip:
#endif
	shm_offset = get_offset(sis->mapper, offset);


	if(swap_switched == 0){
		if(gsis->disk_end - gsis->disk_start > memswap_chunk){
			switch_to_next_swap();
			swap_switched = 1;
		}
	}

	/*
	if(almost_full()){
		if(swap_switched == 0){
			if(add_memswap_chunk() == -1){
				switch_to_next_swap();
				swap_switched = 1;
			}
		}
	}
	*/


	memcpy((char *)sis->shm + shm_offset*entry_size, (char *)kmap(page), PAGE_SIZE);
	kunmap(page);
        unlock_page(page);
	if(offset > sis->disk_end) {
		sis->disk_end = offset;
	}

	if(swap_mounted == 0)
		swap_mounted = 1;

	return ret;
}

int mempipe_swap_readpage(struct page *page)
{
	int ret = 0;
	struct swap_info_struct *sis = page_swap_info(page);
	swp_entry_t entry;
	pgoff_t offset;
	unsigned long shm_offset;

	BUG_ON(sis->shm == NULL);
	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(PageUptodate(page), page);
	if (frontswap_load(page) == 0) {
		SetPageUptodate(page);
		unlock_page(page);
		goto out;
	}

	if (sis->flags & SWP_FILE) {
		printk("Error: readpage from swap file\n");
	}
	
	entry.val = page_private(page);
	offset = swp_offset(entry);


	count_vm_event(PSWPIN);
	shm_offset = get_offset(sis->mapper, offset);
	memcpy((char *)kmap(page), (char *)sis->shm + shm_offset*entry_size, PAGE_SIZE);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	
out:
	return ret;
}
