#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/fs.h>

#include "zdev.h"
#include "uapi/qvio-l4t.h"

static struct qvio_cdev_class __cdev_class;

static void __device_free(struct kref *ref);
static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg);
static long __file_ioctl_g_ticks(struct file * filp, unsigned long arg);

static const struct file_operations __fops = {
	.owner = THIS_MODULE,
	.open = qvio_cdev_open,
	.release = qvio_cdev_release,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
};

static inline void io_write_reg(uintptr_t BaseAddress, int RegOffset, u32 Data) {
    iowrite32(Data, (volatile void *)(BaseAddress + RegOffset));
}

static inline u32 io_read_reg(uintptr_t BaseAddress, int RegOffset) {
    return ioread32((const volatile void *)(BaseAddress + RegOffset));
}

int qvio_zdev_register(void) {
	int err;

	pr_info("\n");

	err = qvio_cdev_register(&__cdev_class, 0, 255, "qenv");
	if(err) {
		pr_err("qvio_cdev_register() failed\n");
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_zdev_unregister(void) {
	pr_info("\n");

	qvio_cdev_unregister(&__cdev_class);
}

struct qvio_zdev* qvio_zdev_new(void) {
	int err;
	struct qvio_zdev* self = kzalloc(sizeof(struct qvio_zdev), GFP_KERNEL);

	if(! self) {
		pr_err("kzalloc() failed\n");
		err = -ENOMEM;
		goto err0;
	}

	kref_init(&self->ref);

	return self;

err0:
	return NULL;
}

struct qvio_zdev* qvio_zdev_get(struct qvio_zdev* self) {
	if (self)
		kref_get(&self->ref);

	return self;
}

static void __device_free(struct kref *ref) {
	struct qvio_zdev* self = container_of(ref, struct qvio_zdev, ref);

	// pr_info("\n");

	kfree(self);
}

void qvio_zdev_put(struct qvio_zdev* self) {
	if (self)
		kref_put(&self->ref, __device_free);
}

int qvio_zdev_probe(struct qvio_zdev* self) {
	int err;

	self->cdev.fops = &__fops;
	self->cdev.private_data = self;
	err = qvio_cdev_start(&self->cdev, &__cdev_class);
	if(err) {
		pr_err("qvio_cdev_start() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

void qvio_zdev_remove(struct qvio_zdev* self) {
	qvio_cdev_stop(&self->cdev, &__cdev_class);
}

ssize_t qvio_zdev_attr_ver_show(struct qvio_zdev* self, char *buf) {
	ssize_t ret;
	uintptr_t reg = (uintptr_t)self->reg;
	u32 nPlatform;

	switch(self->device_id & 0xFFFF) {
	case 0x7024:
		nPlatform = io_read_reg(reg, 0x04);
		ret = snprintf(buf, PAGE_SIZE, "0x%08X %c%c%c%c 0x%08X\n",
			io_read_reg(reg, 0x00),
			(char)((nPlatform >> 24) & 0xFF),
			(char)((nPlatform >> 16) & 0xFF),
			(char)((nPlatform >> 8) & 0xFF),
			(char)((nPlatform >> 0) & 0xFF),
			io_read_reg(reg, 0x08));
		break;

	default:
		nPlatform = io_read_reg(reg, 0x14);
		ret = snprintf(buf, PAGE_SIZE, "0x%08X %c%c%c%c 0x%08X\n",
			io_read_reg(reg, 0x10),
			(char)((nPlatform >> 24) & 0xFF),
			(char)((nPlatform >> 16) & 0xFF),
			(char)((nPlatform >> 8) & 0xFF),
			(char)((nPlatform >> 0) & 0xFF),
			io_read_reg(reg, 0x18));
		break;
	}

	return ret;
}

ssize_t qvio_zdev_attr_ticks_show(struct qvio_zdev* self, char *buf) {
	ssize_t ret;
	uintptr_t reg = (uintptr_t)self->reg;

	switch(self->device_id & 0xFFFF) {
	case 0x7024:
		ret = snprintf(buf, PAGE_SIZE, "%u\n", io_read_reg(reg, 0x0C));
		break;

	default:
		ret = snprintf(buf, PAGE_SIZE, "%u\n", io_read_reg(reg, 0x1C));
		break;
	}

	return ret;
}

ssize_t qvio_zdev_attr_value0_show(struct qvio_zdev* self, char *buf) {
	ssize_t ret;
	uintptr_t reg = (uintptr_t)self->reg;

	switch(self->device_id & 0xFFFF) {
	case 0x7024:
		ret = snprintf(buf, PAGE_SIZE, "%u\n", io_read_reg(reg, 0x14));
		break;

	default:
		ret = snprintf(buf, PAGE_SIZE, "%u\n", 0);
		break;
	}

	return ret;
}

ssize_t qvio_zdev_attr_value0_store(struct qvio_zdev* self, const char *buf, size_t count) {
	uintptr_t reg = (uintptr_t)self->reg;
	int value0;

	sscanf(buf, "%d", &value0);

	switch(self->device_id & 0xFFFF) {
	case 0x7024:
		io_write_reg(reg, 0x14, value0);
		break;

	default:
		break;
	}

	return count;
}

static long __file_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	long ret;

	switch(cmd) {
	case QVIO_IOC_G_TICKS:
		ret = __file_ioctl_g_ticks(filp, arg);
		break;

	default:
		pr_err("unexpected, cmd=%d\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long __file_ioctl_g_ticks(struct file * filp, unsigned long arg) {
	long ret;
	struct qvio_zdev* self = filp->private_data;
	struct qvio_g_ticks args;
	uintptr_t reg = (uintptr_t)self->reg;

	switch(self->device_id) {
	case 0x7024:
		args.ticks = io_read_reg(reg, 0x0C);
		break;

	default:
		args.ticks = io_read_reg(reg, 0x1C);
		break;
	}

	ret = copy_to_user((void __user *)arg, &args, sizeof(args));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err0;
	}

	return 0;

err0:
	return ret;
}
