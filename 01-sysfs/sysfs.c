/*
 * ycsysfs - demo sysfs interface
 * abbr.	ys: ycsysfs
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>

/* ys: ycsysfs */

#define YS_NAME	"ycsysfs"

extern int __init yckobject_init(const char *name);
extern void __exit yckobject_exit(void);

static int __init ycsysfs_init(void)
{
	int err = yckobject_init(YS_NAME);
	if (err) {
		printk(KERN_ERR "ycsysfs: initialize fail\n");
	} else {
		printk(KERN_INFO "ycsysfs: initialize succ\n");
	}

	return err;
}

static void __exit ycsysfs_exit(void)
{
	yckobject_exit();
	printk(KERN_INFO "ycsysfs: destroy");
}

module_init(ycsysfs_init);
module_exit(ycsysfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
MODULE_VERSION("v1.0");
