/*
 * blocksimple.c
 *
 * block device driver demo
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

#define NAME	"blocksimple"

static int major;
module_param(major, int, S_IRUGO);
MODULE_PARM_DESC(major, "blocksimple device major number");

static int partinum = 16;
module_param(partinum, int, S_IRUGO);
MODULE_PARM_DESC(partinum, "max partitions number per blocksimple device");

static unsigned int devnum = 4;
module_param(devnum, uint, S_IRUGO);
MODULE_PARM_DESC(devnum, "number of blocksimple devices");

struct blocksimple_device {
	int number;
	struct request_queue	*queue;
	struct gendisk		*disk;
	struct list_head	list;
};

static const struct block_device_operations bsd_fops = {
	.owner = THIS_MODULE,
};

static LIST_HEAD(blocksimple_devices);
static DEFINE_MUTEX(blocksimple_devices_mutex);

static int bsd_make_request(struct request_queue *q, struct bio *bio)
{
	bio_endio(bio, 0);
	return 0;
}

static struct blocksimple_device *setup_device(unsigned int i)
{
	struct blocksimple_device *bsd;
	struct gendisk *disk;

	bsd = kzalloc(sizeof *bsd, GFP_KERNEL);
	if (!bsd) {
		printk(KERN_ERR "%s: kzalloc failed\n", __func__);
		goto err_out_kzalloc;
	}

	bsd->number = i;
	bsd->queue = blk_alloc_queue(GFP_KERNEL);
	if (!bsd->queue) {
		printk(KERN_ERR "%s: blk_alloc_queue failed\n", __func__);
		goto err_out_queue;
	}
	blk_queue_make_request(bsd->queue, bsd_make_request);
	blk_queue_max_hw_sectors(bsd->queue, 1024);
	blk_queue_bounce_limit(bsd->queue, BLK_BOUNCE_ANY);

	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, bsd->queue);

	disk = bsd->disk = alloc_disk(16);
	if (!disk) {
		printk(KERN_ERR "%s: alloc_disk(%u) failed\n",
		       __func__, partinum);
		goto err_out_disk;
	}
	disk->major = major;
	disk->first_minor = i<<4;
	disk->fops = &bsd_fops;
	disk->private_data = bsd;
	disk->queue = bsd->queue;
	disk->flags = GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "blocksimple%u", i);
	set_capacity(disk, 10240);

	return bsd;

err_out_disk:
	blk_cleanup_queue(bsd->queue);
err_out_queue:
	kfree(bsd);
err_out_kzalloc:
	return NULL;
}

static void free_device(struct blocksimple_device *dev)
{
	put_disk(dev->disk);
	blk_cleanup_queue(dev->queue);
	kfree(dev);
}

static int __init blocksimple_init(void)
{
	int err;
	unsigned int i;
	struct blocksimple_device *bsd, *next;

	if ((err = register_blkdev(major, NAME)) < 0) {
		printk(KERN_ERR "register_blkdev(%d, %s) fail\n", major, NAME);
		return err;
	}
	if (!major)
		major = err;

	for (i = 0; i < devnum; ++i) {
		bsd = setup_device(i);
		if (!bsd)
			goto err_out_kzalloc;

		list_add_tail(&bsd->list, &blocksimple_devices);
	}

	list_for_each_entry(bsd, &blocksimple_devices, list)
		add_disk(bsd->disk);

	printk(KERN_INFO "blocksimple initialized\n");
	return 0;

err_out_kzalloc:
	list_for_each_entry_safe(bsd, next, &blocksimple_devices, list) {
		list_del(&bsd->list);
		free_device(bsd);
	}
	unregister_blkdev(major, NAME);

	return -ENOMEM;
}

static void __exit blocksimple_exit(void)
{
	struct blocksimple_device *bsd, *next;

	list_for_each_entry_safe(bsd, next, &blocksimple_devices, list) {
		list_del(&bsd->list);
		del_gendisk(bsd->disk);
		free_device(bsd);
	}
	unregister_blkdev(major, NAME);
	printk(KERN_INFO "blocksimple destroyed\n");
}

module_init(blocksimple_init);
module_exit(blocksimple_exit);

MODULE_ALIAS("blocksimple");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("module block device driver");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
