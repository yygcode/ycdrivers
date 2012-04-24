/*
 * demo kobject_* routines
 */

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

static struct kobject *kobj_caa;

static int caa_1_val[3] = { 0, 1, 2 };

static ssize_t caa_1_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	int err = -ENOENT;
	const char *name = attr_name(*attr);

	BUG_ON(!name);

	if (0 == strncmp(name, "caa_1_", 6)) {
		int idx = name[6] - '0';
		switch (idx) {
		case 0:
		case 1:
		case 2:
			err = sprintf(buf, "%d\n", caa_1_val[idx]);
			break;
		}
	}

	return err;
}

static ssize_t caa_1_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int err = -ENOENT;
	const char *name = attr_name(*attr);

	BUG_ON(!name);

	if (0 == strncmp(name, "caa_1_", 6)) {
		int idx = name[6] - '0';
		switch (idx) {
		case 0:
		case 1:
		case 2:
			sscanf(buf, "%d", &caa_1_val[idx]);
			err = count;
			break;
		}
	}

	return err;
}

static struct kobj_attribute attrs_caa_1_0 =
	__ATTR(caa_1_0, S_IRUGO|S_IWUGO, caa_1_show, caa_1_store);
static struct kobj_attribute attrs_caa_1_1 =
	__ATTR(caa_1_1, S_IRUGO|S_IWUGO, caa_1_show, caa_1_store);
static struct kobj_attribute attrs_caa_1_2 =
	__ATTR(caa_1_2, S_IRUGO|S_IWUGO, caa_1_show, caa_1_store);

static struct attribute *attrs_caa[] = {
	&attrs_caa_1_0.attr,
	&attrs_caa_1_1.attr,
	&attrs_caa_1_2.attr,
	NULL,
};

static struct attribute_group attrg_caa = {
	.attrs = attrs_caa,
};

int __init yckobject_init(const char *name)
{
	int err = -ENOMEM;

	char *s = kzalloc(32, GFP_KERNEL);

	if (!s)
		goto err_out;

	if (!name)
		name = "yckobj";

	err = snprintf(s, 31, "%s-%s", name, "caa");
	s[err] = '\0';	/* prevent overflow */

	if (!(kobj_caa = kobject_create_and_add(s, kernel_kobj))) {
		printk(KERN_ERR "yckobj: kobject_create_and_add %s fail\n", s);
		goto err_out_caa;
	}

	if ((err = sysfs_create_group(kobj_caa, &attrg_caa))) {
		printk(KERN_ERR "yckobj: sysfs_create_group %s fail\n", s);
		goto err_out_attrs;
	}

	kfree(s);
	return 0;

err_out_attrs:
	kobject_put(kobj_caa);
err_out_caa:
	kfree(s);
err_out:
	return err;
}

void __exit yckobject_exit(void)
{
	sysfs_remove_group(kobj_caa, &attrg_caa);
	kobject_put(kobj_caa);
}
