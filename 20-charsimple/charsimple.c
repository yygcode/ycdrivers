/*
 * filename
 *
 * description
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>

#define YC_CHARSIMPLE_NAME "yc-charsimple"
static int major;
module_param(major, int, S_IRUGO);
MODULE_PARM_DESC(major, "major device number");

static int minor;
module_param(minor, int, S_IRUGO);
MODULE_PARM_DESC(major, "minor device number");

static unsigned int devnum = 4;
module_param(devnum, int, S_IRUGO);
MODULE_PARM_DESC(devnum, "number of devices");

static struct class *charsimple_class;
static const struct file_operations charsimple_fops = {
	.owner = THIS_MODULE,
};

struct  charsimple_cdev {
	struct cdev cdev;
};
static struct charsimple_cdev *charsimple_devices;

static char *charsimple_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "yc/%s", dev_name(dev));
}

static int __init charsimple_init(void)
{
	int err = -EINVAL;
	unsigned int i;
	dev_t dev;

	if (!devnum) {
		printk(KERN_ERR "devnum can not be zero\n");
		goto err_out;
	}

	if (major) {
		dev = MKDEV(major, minor);
		err = register_chrdev_region(dev, devnum, YC_CHARSIMPLE_NAME);
	} else {
		err = alloc_chrdev_region(&dev, minor, devnum,
					  YC_CHARSIMPLE_NAME);
	}

	if (err) {
		printk(KERN_ERR "chrdev region failed, major = %d\n", major);
		goto err_out;
	}

	major = MAJOR(dev);
	minor = MINOR(dev);

	charsimple_devices = kzalloc(devnum * sizeof(struct charsimple_cdev),
				     GFP_KERNEL);

	if (!charsimple_devices) {
		printk(KERN_ERR "kzalloc devices failed\n");
		goto err_out_kzalloc;
	}

	charsimple_class = class_create(THIS_MODULE, "charsimple");
	if (IS_ERR(charsimple_class)) {
		printk(KERN_ERR "class_create mem fail\n");
		err = PTR_ERR(charsimple_class);
		goto err_out_class_create;
	}

	charsimple_class->devnode = charsimple_devnode;

	for(i = 0; i < devnum; ++i) {
		dev_t devt = MKDEV(major, minor + i);
		cdev_init(&charsimple_devices[i].cdev, &charsimple_fops);

		err = cdev_add(&charsimple_devices[i].cdev, devt, 1);
		if (err) {
			printk(KERN_ERR "cdev_add %d fail\n", i);
			goto err_out_cdev_add;
		}

		device_create(charsimple_class, NULL, devt, NULL,
			      "charsimple%d", i);
	}

	printk(KERN_INFO "yccharsimple initialized\n");

	return 0;

err_out_cdev_add:
	while (i) {
		--i;
		device_destroy(charsimple_class, MKDEV(major, minor + i));
		cdev_del(&charsimple_devices[i].cdev);
	}
	class_destroy(charsimple_class);
err_out_class_create:
	kfree(charsimple_devices);
err_out_kzalloc:
	unregister_chrdev_region(dev, devnum);
err_out:
	return err;
}

static void __exit charsimple_exit(void)
{
	unsigned int i;

	for (i = 0; i < devnum; ++i) {
		device_destroy(charsimple_class, MKDEV(major, minor + i));
		cdev_del(&charsimple_devices[i].cdev);
	}
	class_destroy(charsimple_class);
	kfree(charsimple_devices);
	unregister_chrdev_region(MKDEV(major, minor), devnum);

	printk(KERN_INFO "yccharsimple destroyed\n");
}

module_init(charsimple_init);
module_exit(charsimple_exit);

/*
EXPORT_SYMBOL(sym);
EXPORT_SYMBOL_GPL(sym);
*/
MODULE_ALIAS("charsimple");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("yc char simple module");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
