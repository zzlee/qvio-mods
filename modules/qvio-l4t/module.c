#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/init.h>
#include <linux/module.h>

#include "version.h"
#include "cdev.h"
#include "platform_device.h"

#define DRV_MODULE_DESC		"QCAP Video I/O Driver"

static char version[] = DRV_MODULE_DESC " v" DRV_MODULE_VERSION;

MODULE_AUTHOR("ZzLab");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("GPL");

static int __init qvio_mod_init(void)
{
	int err;

	pr_info("%s\n", version);

	err = qvio_cdev_register();
	if (err != 0) {
		pr_err("qvio_cdev_register() failed, err=%d\n", err);
		goto err0;
	}

	err = qvio_device_platform_register();
	if (err != 0) {
		pr_err("qvio_device_platform_register() failed, err=%d\n", err);
		goto err1;
	}

	return 0;

err1:
	qvio_cdev_unregister();
err0:
	return err;
}

static void __exit qvio_mod_exit(void)
{
	pr_info("%s\n", version);

	qvio_device_platform_unregister();
	qvio_cdev_unregister();
}

module_init(qvio_mod_init);
module_exit(qvio_mod_exit);
