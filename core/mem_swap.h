#include<linux/mm.h>
#include<linux/fs.h>
#include<linux/swap.h>
#include<linux/aio.h>
#include<linux/pagemap.h>
#include<linux/swapops.h>
#include<linux/frontswap.h>
#include<linux/swap.h>
#include <linux/gfp.h>
#include <linux/freezer.h>
#include <linux/bio.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/writeback.h>
#include <linux/rmap.h>


//#define MY_ARRAY_SIZE 10000000
//#define TOTAL_SWAP_SHM 262144*1 //maximum size of the shared memory used for swap
//#define SWAP_SHM_SIZE 262144*1 //when to start dumping pages to the disk
struct dump_args{
        unsigned long start, end;
};

struct mdata{
	char exist;
	struct swapin_mdata sm;
};	

int mempipe_swap_init(struct swap_info_struct *sis);

struct swapin_mdata* get_swapin_mdata(unsigned long offset);

int mempipe_swap_writepage(struct page *page, struct writeback_control *wbc, void (*end_write_func)(struct bio *, int));

int mempipe_swap_readpage(struct page *page);

void end_dump_page(struct bio *bio, int err);

int mempipe_dump_shm(void *args);
