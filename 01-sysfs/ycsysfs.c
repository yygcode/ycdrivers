/*
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

/* ys: ycsysfs */
static struct class *ys_class;

#define YS_NAME	"ycsysfs"

static int __init ycsysfs_init(void)
{
	ys_class = class_create(THIS_MODULE, YS_NAME);

	if (IS_ERR(ys_class)) {
		printk(KERN_ERR "ycsysfs: class_create fail");
		return PTR_ERR(ys_class);
	}

	printk(KERN_INFO "ycsysfs: initialize");

	return 0;
}

static void __exit ycsysfs_exit(void)
{
	class_destroy(ys_class);
	printk(KERN_INFO "ycsysfs: destroy");
}

module_init(ycsysfs_init);
module_exit(ycsysfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
MODULE_VERSION("v1.0");
