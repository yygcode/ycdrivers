/*
 * filename
 *
 * description
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>

static int para_int;
module_param(para_int, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(para_int, " para int type");

static unsigned long para_ulong;
module_param(para_ulong, ulong, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(para_ulong, " para unsigned long type");

static char *para_charp;
module_param(para_charp, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(para_charp, " para charp type");

static int para_int_array[4] = { 1, 2, 3, 4 };
module_param_array(para_int_array, int, NULL, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(para_int_array, " para int array type");

static int __init paras_init(void)
{
	printk(KERN_INFO "para_int	= %d\n", para_int);
	printk(KERN_INFO "para_ulong	= %lu\n", para_ulong);
	printk(KERN_INFO "para_charp	= %s\n", para_charp);

	printk(KERN_INFO "ycparas initialized\n");
	return 0;
}

static void __exit paras_exit(void)
{
	printk(KERN_INFO "ycparas destroyed\n");
}

module_init(paras_init);
module_exit(paras_exit);

MODULE_ALIAS("paras");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("parameters example");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
