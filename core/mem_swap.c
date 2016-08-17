#include "mem_swap.h"
#include "mapper.h"
#include "file_ops.h"
#include <linux/swapfile.h>
#include <linux/preempt.h>

extern void end_swap_bio_read(struct bio *bio, int err);

#define SHM_DUMP
#define PROACTIVE_SWAP

/*OFFSET_1 and OFFSET_2 are used to store the swap-in meta data*/
#define OFFSET_1 1048576 //4G from the start of the shm
#define OFFSET_2 1310720 //5G from the start of the shm

#define DUMP_PERCENT 0.7

extern void __iomem * Nahanni_mem;
extern spinlock_t swap_lock;
static struct swap_info_struct *gsis;
static unsigned long pages_to_dump, pages_dumped;


static DECLARE_WAIT_QUEUE_HEAD(dump_thread_wait);
static DECLARE_WAIT_QUEUE_HEAD(swap_thread_wait);
static DECLARE_WAIT_QUEUE_HEAD(IO_thread_wait);

static int init = 0;

struct dump_args *dump_args;

unsigned long memswap_size_total = (1<<21); /*# of pages*/
unsigned long memswap_size = (1<<16); /*# of pages*/

struct task_struct *mempipe_dump_shm_thread;

int memswap_init(struct swap_info_struct *sis){
	/*Offset of the first free shm chunk, in terms of pages. This meta data is the first "long of the whole shm area*/
	unsigned long offset;
	char *start;
	int ret = 0;

	if(Nahanni_mem == NULL){
		printk(KERN_ERR "Nahanni_mem = NULL\n");
		return -1;
	}
	
	sis->mapper = init_mapper(8); /*For now, a shared memory swap partition consists of at most 8 sections*/

	if(sis->mapper == NULL){
		printk(KERN_ERR "sis->mapper initialization fails\n");
		return -1;
	}

	offset = *(unsigned long *)Nahanni_mem;
	
	if(offset == 0){
		offset = 1;
	}

	insert_mapper(sis->mapper, offset);

	printk("%s, offset = %ld\n", __func__, offset);

	start = (char *)Nahanni_mem + offset*PAGE_SIZE;
	memset((char *)start, '0', memswap_size*PAGE_SIZE);

	offset += memswap_size_total;
	*(unsigned long *)Nahanni_mem = offset;
	
	sis->shm = start;
	sis->shm_start = sis->shm_end = 1;
	sis->disk_start = sis->disk_end = 1; 
	sis->mask = 1;
	sis->is_dump = 0;
	sis->swap_IO_finish = 1;
	sis->dump_thread_should_run = 0;

	gsis = sis;
	
	pages_to_dump = pages_dumped = 0;

	dump_args = (struct dump_args *)kmalloc(sizeof(struct dump_args), GFP_KERNEL);	
	if(dump_args) {
		dump_args->sis = sis;
	}
	mempipe_dump_shm_thread = kthread_run(mempipe_dump_shm, dump_args, "mempipe_dump");
	printk("before init return, mapper = %p\n", sis->mapper);
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

int shm_almost_full(void){
	return ((gsis->disk_end - gsis->disk_start) >= memswap_size*DUMP_PERCENT);
}


void end_dump_page(struct bio *bio, int err)
{
        const int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
        struct page *page = bio->bi_io_vec[0].bv_page;

        if(err)
                printk(KERN_CRIT"~~~~~~~~~~~~~~~~~~~~~~~~err = %d\n", err);

        if (!uptodate) {
                SetPageError(page);
                /*
                 * We failed to write the page out to swap-space.
                 * Re-dirty the page in order to avoid it being reclaimed.
                 * Also print a dire warning that things will go BAD (tm)
                 * very quickly.
                 *
                 * Also clear PG_reclaim to avoid rotate_reclaimable_page()
                 */
                set_page_dirty(page);
                printk(KERN_ALERT "Write-error on swap-device (%u:%u:%Lu)\n",
                                imajor(bio->bi_bdev->bd_inode),
                                iminor(bio->bi_bdev->bd_inode),
                                (unsigned long long)bio->bi_iter.bi_sector);
                ClearPageReclaim(page);
        }
        end_page_writeback(page);
        bio_put(bio);
        __free_pages(page, 0);
        pages_dumped++;

        if(pages_dumped == pages_to_dump){
		gsis->disk_start += pages_dumped;
	        pages_dumped = pages_to_dump = 0;
		gsis->swap_IO_finish = 1;
                wake_up_all(&IO_thread_wait);
        }
}


int mempipe_dump_shm(void *args)
{
	struct dump_args *dump_args = (struct dump_args *)args;
	struct swap_info_struct *sis = dump_args->sis;
	unsigned long offset, tmp_offset, start, end;
	struct bio *bio;
	struct page *page;
	int ret = 0;

	while(!kthread_should_stop()) {
		
		if(kthread_should_stop())
			break;
	
		wait_event_interruptible(dump_thread_wait, (gsis->dump_thread_should_run == 1));
	

		start = dump_args->start;
                end = dump_args->end;
                pages_to_dump = end - start;

		gsis->swap_IO_finish = 0;

                for(offset = start; offset < end; offset++) {
                        page = alloc_pages(GFP_KERNEL, 0);
                        if(!page)
                                printk(KERN_CRIT "page allocation failed\n");

                        lock_page(page);

                        tmp_offset = offset;
                        memcpy(page_address(page), (char *)Nahanni_mem + tmp_offset*PAGE_SIZE, PAGE_SIZE);

                        bio = bio_alloc(GFP_NOIO, 1);
                        if(bio) {
                                bio->bi_iter.bi_sector = offset;
                                bio->bi_iter.bi_sector <<= PAGE_SHIFT - 9;
                                bio->bi_bdev = sis->bdev;
                                bio->bi_io_vec[0].bv_page = page;
                                bio->bi_io_vec[0].bv_len = PAGE_SIZE;
                                bio->bi_io_vec[0].bv_offset = 0;
                                bio->bi_vcnt = 1;
                                bio->bi_iter.bi_size = PAGE_SIZE;
                                bio->bi_end_io = end_dump_page;
                        }else{
                                printk(KERN_CRIT "ERRPR: bio is NULL, allocation failed\n");
                                ret = -1;
                        }

                        set_page_writeback(page);
                        unlock_page(page);
                        submit_bio(WRITE, bio);
                }

		wait_event_interruptible(IO_thread_wait, (gsis->swap_IO_finish == 1));
                gsis->is_dump = 0;
                gsis->dump_thread_should_run = 0;
		wake_up_all(&swap_thread_wait);
	}
	return ret;
}



int mempipe_swap_writepage(struct page *page, struct writeback_control *wbc,
void (*end_write_func)(struct bio *, int))
{
        struct bio *bio;
        int ret = 0, rw = WRITE;

        struct swap_info_struct *sis = page_swap_info(page);
	pgoff_t offset;
        swp_entry_t entry;
	char *mdata_exist;

        if (sis->flags & SWP_FILE) {
                struct kiocb kiocb;
                struct file *swap_file = sis->swap_file;
                struct address_space *mapping = swap_file->f_mapping;
                struct iovec iov = {
                        .iov_base = kmap(page),
                        .iov_len  = PAGE_SIZE,
                };

                init_sync_kiocb(&kiocb, swap_file);
                kiocb.ki_pos = page_file_offset(page);
                kiocb.ki_nbytes = PAGE_SIZE;

                set_page_writeback(page);
                unlock_page(page);
                ret = mapping->a_ops->direct_IO(KERNEL_WRITE,
                                                &kiocb, &iov,
                                                kiocb.ki_pos, 1);
                kunmap(page);
                if (ret == PAGE_SIZE) {
                        count_vm_event(PSWPOUT);
                        ret = 0;
                } else {
                        /*
                         * In the case of swap-over-nfs, this can be a
                         * temporary failure if the system has limited
                         * memory for allocating transmit buffers.
                         * Mark the page dirty and avoid
                         * rotate_reclaimable_page but rate-limit the
                         * messages but do not flag PageError like
                         * the normal direct-to-bio case as it could
                         * be temporary.
                         */
                        set_page_dirty(page);
                        ClearPageReclaim(page);
                        pr_err_ratelimited("Write error on dio swapfile (%Lu)\n",
                                page_file_offset(page));
                }
                end_page_writeback(page);
                return ret;
        }

        entry.val = page_private(page);
        offset = swp_offset(entry);
	if(init == 0){
		printk("start memswap_init()\n");
		ret = memswap_init(sis); 
		init = 1;
	}

	BUG_ON(sis->mapper == NULL);

#ifdef SHM_DUMP
	if(offset < sis->shm_start){
		printk("******This swap slot is on the disk\n");
                //The swap slot is on the disk
                set_page_writeback(page);

                bio = bio_alloc(GFP_NOIO, 1);
                if (bio == NULL) {
                        set_page_dirty(page);
                        return -ENOMEM;
                        goto out;
                }

                if (bio) {
                        bio->bi_iter.bi_sector = offset;
                        bio->bi_iter.bi_sector <<= PAGE_SHIFT - 9;
                        bio->bi_bdev = sis->bdev;
                        bio->bi_io_vec[0].bv_page = page;
                        bio->bi_io_vec[0].bv_len = PAGE_SIZE;
                        bio->bi_io_vec[0].bv_offset = 0;
                        bio->bi_vcnt = 1;
                        bio->bi_iter.bi_size = PAGE_SIZE;
                        bio->bi_end_io = end_write_func;
                }

                if(wbc->sync_mode == WB_SYNC_ALL)
                        rw |= REQ_SYNC;
                count_vm_event(PSWPOUT);
                unlock_page(page);
                submit_bio(rw, bio);
                goto out;
        }

	if(current == mempipe_dump_shm_thread) {
                goto shmem;
        }

	if(gsis->is_dump == 1){
		wait_event_interruptible(swap_thread_wait, (gsis->is_dump == 0));
	}else{
		if(shm_almost_full()){
			dump_args->start = sis->disk_start;
			dump_args->end = sis->disk_end;
			
			preempt_disable();
			if(gsis->is_dump == 1){
				wait_event_interruptible(swap_thread_wait, (gsis->is_dump = 0));
			}else{
				gsis->dump_thread_should_run = 1;
				wake_up_interruptible(&dump_thread_wait);
				gsis->is_dump = 1;
				wait_event_interruptible(swap_thread_wait, (gsis->is_dump == 0));
			}
			preempt_enable();
		}
	}
	
#endif

shmem:
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

                mdata->vma = vma;
                mdata->pmd = pmd;
                mdata->address = address;


                *mdata_exist = '1';
                memcpy((char *)sis->shm + OFFSET_2*PAGE_SIZE + offset*sizeof(struct swapin_mdata), (char *)mdata, sizeof(struct swapin_mdata));

                kfree(mdata);
        }
#endif

	//unsigned long Nahanni_offset = get_offset(sis->mapper, offset);
	unsigned long Nahanni_offset = offset + 1;
	memcpy((char *)sis->shm + Nahanni_offset*PAGE_SIZE, (char *)kmap(page), PAGE_SIZE);
	kunmap(page);
        unlock_page(page);
        //end_page_writeback(page);
	if(offset > sis->shm_end) {
		sis->shm_end = offset;
		sis->disk_end = sis->shm_end;
	}

out:
	return ret;
}

int mempipe_swap_readpage(struct page *page)
{
	struct bio *bio;
	int ret = 0;
	struct swap_info_struct *sis = page_swap_info(page);
	//Added by Qi
	swp_entry_t entry;
	pgoff_t offset;
	//

	BUG_ON(sis->shm == NULL);


	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(PageUptodate(page), page);
	if (frontswap_load(page) == 0) {
		SetPageUptodate(page);
		unlock_page(page);
		goto out;
	}

	if (sis->flags & SWP_FILE) {
		struct file *swap_file = sis->swap_file;
		struct address_space *mapping = swap_file->f_mapping;

		ret = mapping->a_ops->readpage(swap_file, page);
		if (!ret)
			count_vm_event(PSWPIN);
		return ret;
	}

	
	
	//Qi
	entry.val = page_private(page);
	offset = swp_offset(entry);

#ifdef SHM_DUMP 
	if(offset == 0){
		printk("read page, offset = %ld, shm_start = %ld\n", offset, sis->shm_start);
        }
	if(offset < sis->shm_start) {
                bio = bio_alloc(GFP_KERNEL, 1);
                if (bio == NULL) {
                        unlock_page(page);
                        ret = -ENOMEM;
                        goto out;
                }

                if (bio) {
                        bio->bi_iter.bi_sector = offset;
                        bio->bi_iter.bi_sector <<= PAGE_SHIFT - 9;
                        bio->bi_bdev = sis->bdev;
                        bio->bi_io_vec[0].bv_page = page;
                        bio->bi_io_vec[0].bv_len = PAGE_SIZE;
                        bio->bi_io_vec[0].bv_offset = 0;
                        bio->bi_vcnt = 1;
                        bio->bi_iter.bi_size = PAGE_SIZE;
                        bio->bi_end_io = end_swap_bio_read;
                }

                count_vm_event(PSWPIN);
                submit_bio(READ, bio);
                goto out;
        }
#endif

	count_vm_event(PSWPIN);
	//offset = get_offset(sis->mapper, offset);
	offset = offset + 1;
	memcpy((char *)kmap(page), (char *)sis->shm + offset*PAGE_SIZE, PAGE_SIZE);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	
out:
	return ret;
}
