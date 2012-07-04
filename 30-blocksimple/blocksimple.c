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
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/types.h>

#define NAME	"blocksimple"
#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1<<PAGE_SECTORS_SHIFT)

static uint major;
static uint max_part = 16;
static uint nr_bs;
static uint size_bs = 16384;

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
#ifdef CONFIG_BLK_DEV_XIP
	.direct_access = bsd_direct_access,
#endif
};

static LIST_HEAD(blocksimple_devices);
static DEFINE_MUTEX(blocksimple_devices_mutex);

static DEFINE_MUTEX(bsd_mutex);
static struct page *bsd_lookup_page(struct bsd_device *bsd, sector_t sector)
{
	pgoff_t idx;
	struct page *page;

	rcu_read_lock();
	idx = sector >> PAGE_SECTORS_SHIFT;
	page = radix_tree_lookup(&bsd->pages, idx);
	rcu_read_unlock();

	BUG_ON(page && page->index != idx);

	return page;
}

static struct page *bsd_insert_page(struct bsd_device *bsd, sector_t sector)
{
	pgoff_t idx;
	struct page *page;
	gfp_t gfp_flags;

	page = bsd_lookup_page(bsd, sector);
	if (page)
		return page;

	gfp_flags = GFP_NOIO | __GFP_ZERO;
#ifndef CONFIG_BLK_DEV_XIP
	gfp_flags |= __GFP_HIGHMEM;
#endif
	page = alloc_page(gfp_flags);
	if (!page)
		return NULL;

	if (radix_tree_preload(GFP_NOIO)) {
		__free_page(page);
		return NULL;
	}

	spin_lock(&bsd->lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	if (radix_tree_insert(&bsd->pages, idx, page)) {
		__free_page(page);
		page = radix_tree_lookup(&bsd->pages, idx);
		BUG_ON(!page);
		BUG_ON(page->index !=idx);
	} else {
		page->index = idx;
	}
	spin_unlock(&bsd->lock);

	radix_tree_preload_end();

	return page;
}

static void bsd_free_page(struct bsd_device *bsd, sector_t sector)
{
	struct page *page;
	pgoff_t idx;

	spin_lock(&bsd->lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	page = radix_tree_delete(&bsd->pages, idx);
	spin_unlock(&bsd->lock);
	if (page)
		__free_page(page);
}

static void bsd_zero_page(struct bsd_device *bsd, sector_t sector)
{
	struct page *page;
	page = bsd_lookup_page(bsd, sector);
	if (page)
		clear_highpage(page);
}

#define FREE_BATCH 16
static void bsd_free_pages(struct bsd_device *bsd)
{

	ulong pos = 0;
	struct page *pages[FREE_BATCH];

	int nr_pages;

	do {
		uint i;

		nr_pages = radix_tree_gang_lookup(&bsd->pages, (void**)pages,
						  pos, FREE_BATCH);

		for (i = 0; i < nr_pages; ++i) {
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(&bsd->pages, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}

		++pos;

	} while (nr_pages == FREE_BATCH);
}

static int copy_to_bsd_setup(struct bsd_device *bsd, sector_t sector, size_t n)
{
	uint offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	if (!bsd_insert_page(bsd, sector))
		return -ENOMEM;

	if (copy < n) {
		sector += copy >> SECTOR_SHIFT;
		if (!bsd_insert_page(bsd, sector))
			return -ENOMEM;
	}

	return 0;
}

static void discard_from_bsd(struct bsd_device *bsd, sector_t sector, size_t n)
{
	while (n >= PAGE_SIZE) {
		if (0)
			bsd_free_page(bsd, sector);
		else
			bsd_zero_page(bsd, sector);
		sector += PAGE_SIZE >> SECTOR_SHIFT;
		n -= PAGE_SIZE;
	}
}

static void copy_to_bsd(struct bsd_device *bsd, const void *src,
			sector_t sector, size_t n)
{
	struct page *page;
	void *dst;
	uint offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = bsd_lookup_page(bsd, sector);
	BUG_ON(!page);

	dst = kmap_atomic(page, KM_USER1);
	memcpy(dst + offset, src, copy);
	kunmap_atomic(dst, KM_USER1);

	if (copy < n) {
		src += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = bsd_lookup_page(bsd, sector);
		BUG_ON(!page);

		dst = kmap_atomic(page, KM_USER1);
		memcpy(dst, src, copy);
		kunmap_atomic(dst, KM_USER1);
	}
}

static void copy_from_bsd(void *dst, struct bsd_device *bsd,
			  sector_t sector, size_t n)
{
	struct page *page;
	void *src;
	uint offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = bsd_lookup_page(bsd, sector);
	if (page) {
		src = kmap_atomic(page, KM_USER1);
		memcpy(dst, src + offset, copy);
		kunmap_atomic(src, KM_USER1);
	} else {
		memset(dst, 0, copy);
	}

	if (copy < n) {
		dst += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;

		page = bsd_lookup_page(bsd, sector);
		if (page) {
			src = kmap_atomic(page, KM_USER1);
			memcpy(dst, src + offset, copy);
			kunmap_atomic(src, KM_USER1);
		} else {
			memset(dst, 0, copy);
		}
	}
}

static int bsd_do_bvec(struct bsd_device *bsd, struct page *page,
		       uint len, uint off, int rw, sector_t sector)
{
	void *mem;
	int err = 0;

	/* treat read-ahead as normal read */
	if (rw == READA)
		rw = READ;

	if (rw != READ) {
		err = copy_to_bsd_setup(bsd, sector, len);
		if (err)
			goto out;
	}

	mem = kmap_atomic(page, KM_USER0);
	if (rw == READ) {
		copy_from_bsd(mem+off, bsd, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		copy_to_bsd(bsd, mem + off, sector, len);
	}
	kunmap_atomic(mem, KM_USER0);

out:
	return err;
}

static int bsd_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct bsd_device *bsd = bdev->bd_disk->private_data;

	struct bio_vec *bvec;
	sector_t sector;
	int i;
	int err = -EIO;

	sector = bio->bi_sector;
	if (sector + (bio->bi_size >> 9) > get_capacity(bdev->bd_disk))
		goto out;

	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		discard_from_bsd(bsd, sector, bio->bi_size);
		goto out;
	}

	bio_for_each_segment(bvec, bio, i) {
		uint len = bvec->bv_len;

		err = bsd_do_bvec(bsd, bvec->bv_page, len, bvec->bv_offset,
				  bio_rw(bio), sector);
		if (err)
			break;

		sector += len >> 9;
	}
out:
	bio_endio(bio, err);
	return 0;
}

#ifdef CONFIG_BLK_DEV_XIP
static int bsd_direct_access(struct block_device *bdev, sector_t sector,
			     void *kaddr, ulong *pfn)
{
	struct bsd_device *bsd = bdev->bd_disk->private_data;
	struct page *page;

	if (!bsd)
		return -ENODEV;

	if (sector & (PAGE_SECTORS - 1))
		return -EINVAL;

	if (sector + PAGE_SECTORS > get_capacity(bdev->bd_disk))
		return -ERANGE;

	page = bsd_insert_page(bsd, sector);
	if (!page)
		return -ENOMEM;

	*kaddr = page_address(page);
	*pfn = page_to_pfn(page);

	return 0;
}
#endif

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
	bsd->queue->limits.max_discard_sectors = UINT_MAX;
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
