#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>

struct ycbus_dev {
	struct attribute id;
	struct device dev;
};

static inline struct ycbus_dev *to_ycbus_dev(struct device *_dev)
{
	return container_of(_dev, struct ycbus_dev, dev);
}

static ssize_t
ycbus_store_rescan(struct bus_type *bus, const char *buf, size_t count)
{
	return count;
}

static ssize_t
ycbus_show_author(struct bus_type *bus, char *buf)
{
	return sprintf(buf, "yanyg <yygcode@gmail.com>\n");
}

static struct bus_attribute ycbus_bus_attrs[] = {
	__ATTR(rescan, (S_IWUSR|S_IWGRP), NULL, ycbus_store_rescan),
	__ATTR(author, S_IRUGO, ycbus_show_author, NULL),
	__ATTR_NULL,
};

static struct device_attribute ycbus_dev_attrs[] = {
	__ATTR_NULL,
};

static struct driver_attribute ycbus_drv_attrs[] = {
	__ATTR_NULL,
};

struct bus_type ycbus_type = {
	.name		= "ycbus",
	.bus_attrs	= ycbus_bus_attrs,
	.dev_attrs	= ycbus_dev_attrs,
	.drv_attrs	= ycbus_drv_attrs,
};
EXPORT_SYMBOL(ycbus_type);

static int __init ycbus_init(void)
{
	return bus_register(&ycbus_type);
}

static void __exit ycbus_exit(void)
{
	bus_unregister(&ycbus_type);
}

module_init(ycbus_init);
module_exit(ycbus_exit);

MODULE_LICENSE("GPL v2");
