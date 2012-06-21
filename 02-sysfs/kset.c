/*
 * demo kset
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static int yc_uevent_filter(struct kset *kset, struct kobject *kobj);
static const char *yc_uevent_name(struct kset *kset, struct kobject *kobj);
static int yc_uevent(struct kset *kset, struct kobject *kobj,
		     struct kobj_uevent_env *env);

static struct kset_uevent_ops yc_uevent_ops = {
	.filter	= yc_uevent_filter,
	.name	= yc_uevent_name,
	.uevent	= yc_uevent,
};

static struct kset *yc_kset;

static ssize_t yckset_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count);

static ssize_t yckset_version(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf);

static struct kobj_attribute kobj_attrs[] = {
	__ATTR(create, S_IWUSR, NULL, yckset_store),
	__ATTR(link, S_IWUSR, NULL, yckset_store),
	__ATTR(remove, S_IWUSR, NULL, yckset_store),
	__ATTR(version, S_IRUGO, yckset_version, NULL),
};

static int yc_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0;
}

static const char *yc_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "yckset";
}

static int yc_uevent(struct kset *kset, struct kobject *kobj,
		     struct kobj_uevent_env *env)
{
	return 0;
}

static ssize_t yckset_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	return count;
}

static ssize_t yckset_version(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
#ifndef YCKSET_VERSION
#define YCKSET_VERSION	1.0.0
#endif
	return sprintf(buf, "%s\n", __stringify(YCKSET_VERSION));
}

static int __init yckset_init(void)
{
	int i, r = -ENOMEM;

	yc_kset = kset_create_and_add("yckset", &yc_uevent_ops, NULL);
	if (!yc_kset)
		return -ENOMEM;

	for (i = 0; i < sizeof(kobj_attrs)/sizeof(kobj_attrs[0]); ++i) {
		r = sysfs_create_file(&yc_kset->kobj, &kobj_attrs[i].attr);
		if (r)
			break;
	}
	if (r) {
		while (--i >= 0)
			sysfs_remove_file(&yc_kset->kobj, &kobj_attrs[i].attr);
		goto err_out;
	}

	pr_info("yckset init\n");
	return 0;

err_out:
	kset_unregister(yc_kset);
	return r;
}

static void __exit yckset_exit(void)
{
	int i;

	for (i = 0; i < sizeof(kobj_attrs)/sizeof(kobj_attrs[0]); ++i) {
		sysfs_remove_file(&yc_kset->kobj, &kobj_attrs[i].attr);
	}
	kset_unregister(yc_kset);

	pr_info("yckset exit\n");
}

module_init(yckset_init);
module_exit(yckset_exit);

MODULE_ALIAS("yckset");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("kset example");
MODULE_AUTHOR("yanyg <yygcode@gmail.com>");
