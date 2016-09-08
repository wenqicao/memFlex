#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uio_driver.h>
#include <asm/io.h>
#include <linux/swapfile.h>
#include <linux/spinlock.h>
#include "mem_swap.h"
#include "debug.h"
#include "bitmap.h"


#define IntrStatus 0x04
#define IntrMask 0x00

char magic = 0x87;
spinlock_t bitmap_lock;
int meta_size = sizeof(magic) + sizeof(spinlock_t);

extern void back_to_default_swap(void);
extern unsigned long memswap_chunk;
extern spinlock_t init_lock;
extern struct task_struct *swapin_thread;

void __iomem *Nahanni_reg;
void __iomem *Nahanni_mem;

struct ivshmem_info {
	struct uio_info *uio;
	struct pci_dev *dev;
	char (*msix_names)[256];
	struct msix_entry *msix_entries;
	int nvectors;
};

//added by Qi
static struct ivshmem_info *ivs_info;
//

static irqreturn_t ivshmem_handler(int irq, struct uio_info *dev_info)
{

	void __iomem *plx_intscr = dev_info->mem[0].internal_addr
					+ IntrStatus;
	u32 val;
	val = readl(plx_intscr);
	if (val == 0)
		return IRQ_NONE;
	
	return IRQ_HANDLED;
}



static irqreturn_t ivshmem_msix_handler(int irq, void *opaque)
{
	return IRQ_HANDLED;
}

static void free_msix_vectors(struct ivshmem_info *ivs_info,
							const int max_vector)
{
	int i;
	for (i = 0; i < max_vector; i++)
		free_irq(ivs_info->msix_entries[i].vector, ivs_info->uio);
}

static int request_msix_vectors(struct ivshmem_info *ivs_info, int nvectors)
{
	int i, err;
	const char *name = "ivshmem";

	ivs_info->nvectors = nvectors;

	ivs_info->msix_entries = kmalloc(nvectors * sizeof *
						ivs_info->msix_entries,
						GFP_KERNEL);
	if (ivs_info->msix_entries == NULL)
		return -ENOSPC;

	ivs_info->msix_names = kmalloc(nvectors * sizeof *ivs_info->msix_names,
			GFP_KERNEL);
	if (ivs_info->msix_names == NULL) {
		kfree(ivs_info->msix_entries);
		return -ENOSPC;
	}

	for (i = 0; i < nvectors; ++i)
		ivs_info->msix_entries[i].entry = i;

	err = pci_enable_msix(ivs_info->dev, ivs_info->msix_entries,
					ivs_info->nvectors);
	if (err > 0) {
		ivs_info->nvectors = err; /* msi-x positive error code
					 returns the number available*/
		err = pci_enable_msix(ivs_info->dev, ivs_info->msix_entries,
					ivs_info->nvectors);
		if (err) {
			printk(KERN_INFO "no MSI (%d). Back to INTx.\n", err);
			goto error;
		}
	}

	if (err)
	    goto error;
	
	for (i = 0; i < ivs_info->nvectors; i++) {

		snprintf(ivs_info->msix_names[i], sizeof *ivs_info->msix_names,
			"%s-config", name);

		err = request_irq(ivs_info->msix_entries[i].vector,
			ivshmem_msix_handler, 0,
			ivs_info->msix_names[i], ivs_info->uio);

		if (err) {
			free_msix_vectors(ivs_info, i - 1);
			goto error;
		}

	}

	return 0;
error:
	kfree(ivs_info->msix_entries);
	kfree(ivs_info->msix_names);
	return err;

}

//static int __devinit ivshmem_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
static int  ivshmem_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct uio_info *info;
	struct ivshmem_info * ivshmem_info;
	int nvectors = 2;

	info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ivshmem_info = kzalloc(sizeof(struct ivshmem_info), GFP_KERNEL);
	if (!ivshmem_info) {
		kfree(info);
		return -ENOMEM;
	}

	//added by Qi
	ivs_info = ivshmem_info;	
	//

	if (pci_enable_device(dev))
		goto out_free;

	if (pci_request_regions(dev, "ivshmem"))
		goto out_disable;

	info->mem[0].addr = pci_resource_start(dev, 0);
	if (!info->mem[0].addr)
		goto out_release;

	info->mem[0].size = pci_resource_len(dev, 0);
	info->mem[0].internal_addr = pci_ioremap_bar(dev, 0);
	if (!info->mem[0].internal_addr) {
		goto out_release;
	}

	info->mem[0].memtype = UIO_MEM_PHYS;
	//Added by Qi
	Nahanni_reg = info->mem[0].internal_addr;
	printk("Nahanni_reg = %p\n", Nahanni_reg);
	spin_lock_init(&init_lock);
	//end
	info->mem[1].addr = pci_resource_start(dev, 2);
	if (!info->mem[1].addr)
		goto out_unmap;


    info->mem[1].internal_addr = ioremap_cache(pci_resource_start(dev, 2),
				     pci_resource_len(dev, 2));
	if (!info->mem[1].internal_addr)
		goto out_unmap;

#if 0
    info->mem[1].internal_addr = pci_ioremap_bar(dev, 2);
	if (!info->mem[1].internal_addr)
		goto out_unmap;
#endif

	info->mem[1].size = pci_resource_len(dev, 2);
	info->mem[1].memtype = UIO_MEM_PHYS;
	
	
	Nahanni_mem = info->mem[1].internal_addr;
	printk("Nahanni_mem = %p\n", Nahanni_mem);
	bitmap_lock = *(spinlock_t *)((char *)Nahanni_mem + 1);
	
	/*Initialize the first page of Nahanni_mem*/
	/*
	*|magic(char)|spinlock_t|bitmap|pad| (Layout of the first page)
	*/
	if(*(char *)Nahanni_mem != magic){
		printk("~~~~~~~~~~~~~~Init first page of Nahanni_mem\n");
		*(char *)Nahanni_mem = magic;
		spin_lock_init(&bitmap_lock);	
		clear_all_bits((char *)Nahanni_mem + meta_size, (PAGE_SIZE - meta_size)*BITS_PER_BYTE);
	}
	
	ivshmem_info->uio = info;
	ivshmem_info->dev = dev;
	
 	printk("nvectors = %d\n", nvectors);

	if (request_msix_vectors(ivshmem_info, nvectors) != 0) {
		printk(KERN_INFO "regular IRQs\n");
		info->irq = dev->irq;
		info->irq_flags = IRQF_SHARED;
		info->handler = ivshmem_handler;
		writel(0xffffffff, info->mem[0].internal_addr + IntrMask);
	} else {
		printk(KERN_INFO "MSI-X enabled\n");
		info->irq = -1;
	}


	info->name = "ivshmem";
	info->version = "0.0.1";

	if (uio_register_device(&dev->dev, info))
		goto out_unmap2;

	pci_set_drvdata(dev, info);


	return 0;
out_unmap2:
	iounmap(info->mem[2].internal_addr);
out_unmap:
	iounmap(info->mem[0].internal_addr);
out_release:
	pci_release_regions(dev);
out_disable:
	pci_disable_device(dev);
out_free:
	kfree (info);
	return -ENODEV;
}

static void ivshmem_pci_remove(struct pci_dev *dev)
{
	struct uio_info *info = pci_get_drvdata(dev);


	
	//added by Qi
	free_msix_vectors(ivs_info, 2);
	pci_disable_msix(dev);
	//end
	
	uio_unregister_device(info);
	pci_release_regions(dev);
	pci_disable_device(dev);
	pci_set_drvdata(dev, NULL);
	iounmap(info->mem[0].internal_addr);


	kfree (info);
}


//static struct pci_device_id ivshmem_pci_ids[] __devinitdata = {
static struct pci_device_id ivshmem_pci_ids[] = {
	{
		.vendor =	0x1af4,
		.device =	0x1110,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static struct pci_driver ivshmem_pci_driver = {
	.name = "uio_ivshmem",
	.id_table = ivshmem_pci_ids,
	.probe = ivshmem_pci_probe,
	.remove = ivshmem_pci_remove,
};


	typedef int (* swap_writepage_hook)(struct page *page, struct writeback_control *wbc, void (*end_write_func)(struct bio *, int));
	typedef int (* swap_readpage_hook)(struct page *page);
	typedef struct swapin_mdata* (* get_swapin_mdata_hook)(unsigned long offset);

	extern void swap_bind_hook(swap_writepage_hook h1, swap_readpage_hook h2, get_swapin_mdata_hook h3);
	extern void swap_unbind_hook(void);

static int __init ivshmem_init_module(void)
{
	int ret;

	ret = pci_register_driver(&ivshmem_pci_driver);
	
	swap_bind_hook(mempipe_swap_writepage, mempipe_swap_readpage, get_swapin_mdata);
	set_memswap_init_size(memswap_chunk);/*# of pages*/
	DPRINTK("*****************ivshmem init****************\n");

	return ret;
}

static void __exit ivshmem_exit_module(void)
{

	swap_unbind_hook();
	pci_unregister_driver(&ivshmem_pci_driver);
	*(char *)Nahanni_mem = 0x00;
	back_to_default_swap();
	
	if(swapin_thread != NULL)
		kthread_stop(swapin_thread);
	
	DPRINTK("**********ivshmem exit***********\n\n\n\n\n\n\n");
}

module_init(ivshmem_init_module);
module_exit(ivshmem_exit_module);

MODULE_DEVICE_TABLE(pci, ivshmem_pci_ids);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cam Macdonell");
