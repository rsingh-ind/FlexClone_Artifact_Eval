#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void)
{
	printk("<1>Hello world 1.\n");
	// A non 0 return means init_module failed; module can't be loaded.
	return 0;
}
void cleanup_module(void)
{
	printk(KERN_ALERT "Goodbye world 1.\n");
}
MODULE_LICENSE("GPL");
