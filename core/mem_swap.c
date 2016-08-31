#include "mem_swap.h"
#include "mapper.h"
#include "bitmap.h"
#include <linux/swapfile.h>

#define PROACTIVE_SWAP
#define OFFSET_1 1048576 //4G from the start of the shm
#define OFFSET_2 1310720 //5G from the start of the shm
#define TH 0.95

extern void __iomem * Nahanni_mem;
extern spinlock_t bitmap_lock;
extern int meta_size;
extern void switch_to_next_swap(void);

static struct swap_info_struct *gsis;

static int swap_switched = 0;
static int init = 0;
spinlock_t init_lock;

unsigned long shm_total = (1<<21); /*# of pages*/
unsigned long memswap_total = 0; /*# of pages*/
unsigned long memswap_chunk = (1<<19); /*# of pages*/

unsigned long chunk_num;



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

	sis->shm = p + PAGE_SIZE;
	sis->shm_start = sis->shm_end = (unsigned long)(sis->shm + chunk_offset*memswap_chunk*PAGE_SIZE);
        sis->disk_start = sis->disk_end = 0;

	gsis = sis;
	

	return ret;
}

struct swapin_mdata* get_swapin_mdata(unsigned long offset)
{
	char *exist = (char *)gsis->shm + OFFSET_1*PAGE_SIZE + offset;
	if(*exist == '0') {
		return NULL;
	}else{
		struct swapin_mdata *mdata = (struct swapin_mdata *)((char *)gsis->shm + OFFSET_2*PAGE_SIZE + offset*sizeof(struct swapin_mdata));
		return mdata;
	}

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
	char *mdata_exist;


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

	mdata_exist = (char *)sis->shm + OFFSET_1*PAGE_SIZE + offset*sizeof(char);
        *mdata_exist = '0';

#ifdef PROACTIVE_SWAP
        if(page->idx == 1) {
                pgd_t *pgd;
                pud_t *pud;
                pmd_t *pmd;

                struct swapin_mdata *mdata = (struct swapin_mdata*)kmalloc(sizeof(struct swapin_mdata), GFP_KERNEL);
                unsigned long address = page->rmap_addrs[0];
                struct vm_area_struct *vma = page->rmap_vmas[0];

                pgd = pgd_offset(vma->vm_mm, address);
                pud = pud_offset(pgd, address);
                pmd = pmd_offset(pud, address);

		if(pmd == NULL)
			goto free;
	
                mdata->vma = vma;
                mdata->pmd = pmd;
                mdata->address = address;


                *mdata_exist = '1';
                memcpy((char *)sis->shm + OFFSET_2*PAGE_SIZE + offset*sizeof(struct swapin_mdata), (char *)mdata, sizeof(struct swapin_mdata));

free:
                kfree(mdata);
        }
#endif
	shm_offset = get_offset(sis->mapper, offset);


	if(swap_switched == 0){
		if(gsis->disk_end - gsis->disk_start > ((1<<18))){
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

	memcpy((char *)sis->shm + shm_offset*PAGE_SIZE, (char *)kmap(page), PAGE_SIZE);
	kunmap(page);
        unlock_page(page);
	if(offset > sis->disk_end) {
		sis->disk_end = offset;
	}

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
	memcpy((char *)kmap(page), (char *)sis->shm + shm_offset*PAGE_SIZE, PAGE_SIZE);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	
out:
	return ret;
}
