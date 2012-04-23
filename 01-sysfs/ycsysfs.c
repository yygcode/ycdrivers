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
static struct attribute ys_attr1 = {
	.name	= "attr1",
	.mode	= S_IRUGO,
};
static struct attribute g1_attr1 = {
	.name	= "g1_attr1",
	.mode	= S_IRUGO,
};
static struct attribute g1_attr2 = {
	.name	= "g1_attr2",
	.mode	= S_IRUGO,
};
static struct attribute *ys_g1_attr[] = {
	&g1_attr1,
	&g1_attr2,
	NULL,
};

static struct attribute_group ys_attrg1 = {
	.name = "attrg1",
	.attrs = ys_g1_attr,
};

static ssize_t
authors_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "yanyg <yygcode@gmail.com>\n");
}

CLASS_ATTR(authors, S_IRUGO, authors_show, NULL);
CLASS_ATTR_STRING(string, S_IRUGO, "This is a string");

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
		printk(KERN_ERR "ycsysfs: class_create_file author fail\n");
		goto err_out_class_create_file_authors;
	}

	if ((err = class_create_file(ys_class, &class_attr_string.attr))) {
		printk(KERN_ERR "ycsysfs: class_create_file string fail\n");
		goto err_out_class_create_file_string;
	}

	if ((err = sysfs_create_file(ys_class->dev_kobj, &ys_attr1))) {
		printk(KERN_ERR "ycsysfs: sysfs_create_file attr1 fail\n");
		goto err_out_sysfs_create_file_attr1;
	}

	if ((err = sysfs_create_group(ys_class->dev_kobj, &ys_attrg1))) {
		printk(KERN_ERR "ycsysfs: sysfs_create_group attrg1 fail\n");
		goto err_out_sysfs_create_group_attrg1;
	}

	printk(KERN_INFO "ycsysfs: initialize\n");

	return 0;

err_out_sysfs_create_group_attrg1:
	sysfs_remove_file(ys_class->dev_kobj, &ys_attr1);
err_out_sysfs_create_file_attr1:
	class_remove_file(ys_class, &class_attr_string.attr);
err_out_class_create_file_string:
	class_remove_file(ys_class, &class_attr_authors);
err_out_class_create_file_authors:
	class_destroy(ys_class);

	return err;
}

static void __exit ycsysfs_exit(void)
{
	sysfs_remove_group(ys_class->dev_kobj, &ys_attrg1);
	sysfs_remove_file(ys_class->dev_kobj, &ys_attr1);
	class_remove_file(ys_class, &class_attr_string.attr);
	class_remove_file(ys_class, &class_attr_authors);
	class_destroy(ys_class);
	printk(KERN_INFO "ycsysfs: destroy");
}

module_init(ycsysfs_init);
module_exit(ycsysfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
MODULE_VERSION("v1.0");
