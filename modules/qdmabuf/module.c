#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "config.h"

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "version.h"
#include "device.h"
#include "cdev.h"

#define DRV_MODULE_DESC		"QCAP dma-buf Driver"

static char version[] = DRV_MODULE_DESC " v" DRV_MODULE_VERSION;

MODULE_AUTHOR("ZzLab");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif

#if Z_CONFIG_OF
struct of_device_id qdmabuf_of_match[] = {
	{ .compatible = "yuan,qdmabuf", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, qdmabuf_of_match);
#else // Z_CONFIG_OF
static struct platform_device *pdev_qdmabuf;
#endif // Z_CONFIG_OF

static int __init qdmabuf_mod_init(void)
{
	int err;

	pr_info("%s\n", version);

	err = qdmabuf_device_register();
	if (err != 0) {
		pr_err("qdmabuf_device_register() failed, err=%d\n", err);
		goto err0;
	}

#if ! Z_CONFIG_OF
	pdev_qdmabuf = platform_device_alloc(QDMABUF_DRIVER_NAME, 0);
	if (pdev_qdmabuf == NULL) {
		pr_err("platform_device_alloc() failed\n");
		err = -ENOMEM;
		goto err1;
	}

	err = platform_device_add(pdev_qdmabuf);
	if (err != 0) {
		pr_err("platform_device_add() failed, err=%d\n", err);
		err = -ENOMEM;
		goto err2;
	}
#endif // ! Z_CONFIG_OF

	return err;

#if ! Z_CONFIG_OF
err2:
	platform_device_put(pdev_qdmabuf);
err1:
	qdmabuf_device_unregister();
#endif // ! Z_CONFIG_OF
err0:
	return err;
}

static void __exit qdmabuf_mod_exit(void)
{
	pr_info("%s\n", version);

#if ! Z_CONFIG_OF
	platform_device_del(pdev_qdmabuf);
	platform_device_put(pdev_qdmabuf);
#endif // ! Z_CONFIG_OF

	qdmabuf_device_unregister();
}

module_init(qdmabuf_mod_init);
module_exit(qdmabuf_mod_exit);
