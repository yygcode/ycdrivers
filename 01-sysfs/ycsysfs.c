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

static struct class *ys_class;

static ssize_t
authors_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "yanyg <yygcode@gmail.com>\n");
}

CLASS_ATTR(authors, S_IRUGO, authors_show, NULL);

static int __init ycsysfs_init(void)
{
	int err;

	ys_class = class_create(THIS_MODULE, YS_NAME);

	if (IS_ERR(ys_class)) {
		printk(KERN_ERR "ycsysfs: class_create fail\n");
		return PTR_ERR(ys_class);
	}

	/* /sys/class/ycsysfs/author */
	if ((err = class_create_file(ys_class, &class_attr_authors))) {
		printk(KERN_ERR "ycsysfs: sysfs_create_file fail\n");
		goto err_out_sysfs_create_file_authors;
	}

	printk(KERN_INFO "ycsysfs: initialize\n");

	return 0;

err_out_sysfs_create_file_authors:
	class_destroy(ys_class);

	return err;
}

static void __exit ycsysfs_exit(void)
{
	class_remove_file(ys_class, &class_attr_authors);
	class_destroy(ys_class);
	printk(KERN_INFO "ycsysfs: destroy");
}

module_init(ycsysfs_init);
module_exit(ycsysfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
MODULE_VERSION("v1.0");
