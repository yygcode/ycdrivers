/*
 * filename
 *
 * description
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int paras;
module_param(paras, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(paras, "template paras");

static int __init template_init(void)
{
	return 0;
}

static void __exit template_exit(void)
{
}

module_init(template_init);
module_exit(template_exit);

/*
EXPORT_SYMBOL(sym);
EXPORT_SYMBOL_GPL(sym);
*/
MODULE_ALIAS("template_alias");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("module template");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
