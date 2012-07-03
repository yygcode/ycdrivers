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
#include <linux/kdev_t.h>
#include <linux/types.h>

#define NAME	"blocksimple"

static uint major;
static uint max_part = 16;
static uint nr_bs;
static uint size_bs;

module_param(major, uint, S_IRUGO);
MODULE_PARM_DESC(major, "blocksimple device major number");
module_param(max_part, uint, S_IRUGO);
MODULE_PARM_DESC(partinum, "maximum partitions number per blocksimple device");
module_param(nr_bs, uint, S_IRUGO);
MODULE_PARM_DESC(nr_bs, "maximum number of blocksimple devices");
module_param(size_bs, uint, S_IRUGO);
MODULE_PARM_DESC(size_bs, "size of each blocksimple device in kbytes");

static uint part_shift;

struct bsd_device {
	struct request_queue	*queue;
	struct gendisk		*disk;
	struct list_head	list;

	spinlock_t		lock;
	struct radix_tree_root	pages;
};

static const struct block_device_operations bsd_fops = {
	.owner = THIS_MODULE,
};

static LIST_HEAD(blocksimple_devices);
static DEFINE_MUTEX(blocksimple_devices_mutex);

static void bsd_free_pages(struct bsd_device *bsd)
{
}

static int bsd_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	//struct bsd_device *bsdev = bdev->bd_disk->private_data;

	int rw;
	struct bio_vec *bvec;
	sector_t sector;
	int i;
	int err = -EIO;

	sector = bio->bi_sector;
	if (sector + (bio->bi_size >> 9) > get_capacity(bdev->bd_disk))
		goto out;

	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		goto out;
	}

	rw = bio_rw(bio);
	if (rw == READA)
		rw = 0;

	bio_for_each_segment(bvec, bio, i) {
		unsigned int len = bvec->bv_len;
		if (rw == 0) {
			printk(KERN_WARNING "bs read ...\n");
		} else {
			printk(KERN_WARNING "bs write ...\n");
		}

		sector += len >> 9;
	}
out:
	bio_endio(bio, err);
	return 0;
}

static struct bsd_device *bsd_alloc(unsigned int i)
{
	struct bsd_device *bsd;
	struct gendisk *disk;

	bsd = kzalloc(sizeof *bsd, GFP_KERNEL);
	if (!bsd) {
		printk(KERN_ERR "kzalloc bsd failed\n");
		goto err_out_kzalloc;
	}

	spin_lock_init(&bsd->lock);
	INIT_RADIX_TREE(&bsd->pages, GFP_ATOMIC);

	bsd->queue = blk_alloc_queue(GFP_KERNEL);
	if (!bsd->queue) {
		printk(KERN_ERR "blk_alloc_queue failed\n");
		goto err_out_queue;
	}
	blk_queue_make_request(bsd->queue, bsd_make_request);
	blk_queue_max_hw_sectors(bsd->queue, 1024);
	blk_queue_bounce_limit(bsd->queue, BLK_BOUNCE_ANY);

	//bsd->queue->limits.discard_granularity = PAGE_SIZE;
	//bsd->queue->limits.max_discard_sectors = UINT_MAX;
	//bsd->queue->limits.discard_zeroes_data = 1;
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, bsd->queue);

	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, bsd->queue);

	disk = bsd->disk = alloc_disk(1<<part_shift);
	if (!disk) {
		printk(KERN_ERR "alloc_disk(%u) failed\n", max_part);
		goto err_out_disk;
	}
	disk->major = major;
	disk->first_minor = i<<part_shift;
	disk->fops = &bsd_fops;
	disk->private_data = bsd;
	disk->queue = bsd->queue;
	disk->flags = GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "blocksimple%u", i);
	set_capacity(disk, size_bs * 2);

	return bsd;

err_out_disk:
	blk_cleanup_queue(bsd->queue);
err_out_queue:
	kfree(bsd);
err_out_kzalloc:
	return NULL;
}

static void bsd_free(struct bsd_device *bsd)
{
	put_disk(bsd->disk);
	blk_cleanup_queue(bsd->queue);
	bsd_free_pages(bsd);
	kfree(bsd);
}

static struct bsd_device *bsd_init_one(int i)
{
	struct bsd_device *bsd;

	list_for_each_entry(bsd, &blocksimple_devices, list) {
		if (bsd->disk->first_minor == (i << part_shift))
			goto out;
	}

	bsd = bsd_alloc(i);
	if (bsd) {
		add_disk(bsd->disk);
		list_add_tail(&bsd->list, &blocksimple_devices);
	}
out:
	return bsd;
}

static void bsd_del_one(struct bsd_device *bsd)
{
	list_del(&bsd->list);
	del_gendisk(bsd->disk);
	bsd_free(bsd);
}

static struct kobject *bsd_probe(dev_t dev, int *part, void *data)
{
	struct bsd_device *bsd;
	struct kobject *kobj;

	mutex_lock(&blocksimple_devices_mutex);
	bsd = bsd_init_one(MINOR(dev) >> part_shift);
	kobj = bsd ? get_disk(bsd->disk) : ERR_PTR(-ENOMEM);
	mutex_unlock(&blocksimple_devices_mutex);

	*part = 0;
	return kobj;
}

static int __init blocksimple_init(void)
{
	int err;
	uint i, nr;
	ulong range;
	struct bsd_device *bsd, *next;

	if (max_part) {
		part_shift = fls(max_part);
		max_part = (1UL << part_shift) - 1;
	}

	if (max_part > DISK_MAX_PARTS) {
		printk(KERN_ERR "partitions num overflow (%u, %u)\n",
		       max_part, DISK_MAX_PARTS);
		return -EINVAL;
	}

	if (nr_bs > (1UL << (MINORBITS - part_shift))) {
		printk(KERN_ERR "devices number overflow (%ul, %lu)\n",
		       nr_bs, (1UL << (MINORBITS - part_shift)));
		return -EINVAL;
	}

	if (nr_bs) {
		nr = nr_bs;
		range = nr_bs << part_shift;
	} else {
		nr = 4;
		range = 1UL << MINORBITS;
	}

	if ((err = register_blkdev(major, NAME)) < 0) {
		printk(KERN_ERR "register_blkdev(%d, %s) fail\n", major, NAME);
		return err;
	}
	if (!major)
		major = err;

	for (i = 0; i < nr; ++i) {
		bsd = bsd_alloc(i);
		if (!bsd)
			goto err_out_setup;

		list_add_tail(&bsd->list, &blocksimple_devices);
	}

	list_for_each_entry(bsd, &blocksimple_devices, list)
		add_disk(bsd->disk);

	blk_register_region(MKDEV(major, 0), range,
			    THIS_MODULE, bsd_probe, NULL, NULL);

	printk(KERN_INFO "blocksimple initialized\n");
	return 0;

err_out_setup:
	list_for_each_entry_safe(bsd, next, &blocksimple_devices, list) {
		list_del(&bsd->list);
		bsd_free(bsd);
	}
	unregister_blkdev(major, NAME);

	return -ENOMEM;
}

static void __exit blocksimple_exit(void)
{
	ulong range;
	struct bsd_device *bsd, *next;

	list_for_each_entry_safe(bsd, next, &blocksimple_devices, list)
		bsd_del_one(bsd);

	range = nr_bs ? nr_bs << part_shift : 1UL << MINORBITS;
	blk_unregister_region(MKDEV(major, 0), range);
	unregister_blkdev(major, NAME);
	printk(KERN_INFO "blocksimple destroyed\n");
}

module_init(blocksimple_init);
module_exit(blocksimple_exit);

MODULE_ALIAS("blocksimple");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("module block device driver");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
