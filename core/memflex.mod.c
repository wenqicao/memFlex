#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xd1ba7fa7, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xa7a5435d, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x10275d9c, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0xee269af5, __VMLINUX_SYMBOL_STR(set_memswap_init_size) },
	{ 0x9b03b863, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0xbb0493bb, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0xf55f5532, __VMLINUX_SYMBOL_STR(uio_unregister_device) },
	{ 0x53674678, __VMLINUX_SYMBOL_STR(pci_release_regions) },
	{ 0x40c7247c, __VMLINUX_SYMBOL_STR(si_meminfo) },
	{ 0x3d05b2c1, __VMLINUX_SYMBOL_STR(pci_enable_msix) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xe52414, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0xa6d9c176, __VMLINUX_SYMBOL_STR(__frontswap_load) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0xa557fb24, __VMLINUX_SYMBOL_STR(swap_bind_hook) },
	{ 0x5c01aa99, __VMLINUX_SYMBOL_STR(switch_to_next_swap) },
	{ 0xc5fdef94, __VMLINUX_SYMBOL_STR(call_usermodehelper) },
	{ 0xfff385d9, __VMLINUX_SYMBOL_STR(back_to_default_swap) },
	{ 0x2072ee9b, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0xca3876e, __VMLINUX_SYMBOL_STR(unlock_page) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x88f8702a, __VMLINUX_SYMBOL_STR(swap_unbind_hook) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xee46a792, __VMLINUX_SYMBOL_STR(vm_event_states) },
	{ 0x1da88f99, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0xafe7422d, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x8f59692, __VMLINUX_SYMBOL_STR(pci_ioremap_bar) },
	{        0, __VMLINUX_SYMBOL_STR(page_swap_info) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x78980c0e, __VMLINUX_SYMBOL_STR(pci_request_regions) },
	{ 0xa2cb279c, __VMLINUX_SYMBOL_STR(pv_mmu_ops) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0x68c7263, __VMLINUX_SYMBOL_STR(ioremap_cache) },
	{ 0xdcdf162f, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0x3c50e63c, __VMLINUX_SYMBOL_STR(__uio_register_device) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x5ba89b7c, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0x1ab94263, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=uio";

MODULE_ALIAS("pci:v00001AF4d00001110sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "94F092E4CF5BCE26C1DA096");
