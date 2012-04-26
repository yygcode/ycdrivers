/*
 * blocksimple.c
 *
 * block device driver demo
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int paras;
module_param(paras, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(paras, "template paras");

struct blocksimple_device {
	int num;
	struct request_queue	*queue;
	struct gendisk		*disk;
	struct list_head	list;
};

static int __init blocksimple_init(void)
{
	int err;
	return 0;
}

static void __exit blocksimple_exit(void)
{
}

module_init(blocksimple_init);
module_exit(blocksimple_exit);

MODULE_ALIAS("blocksimple");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("module block device driver");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
