#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "umods.h"

#include <linux/compat.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/poll.h>


int qvio_umods_req_entry_new(struct qvio_umods_req_entry** req_entry) {
	int err;

	*req_entry = kzalloc(sizeof(struct qvio_umods_req_entry), GFP_KERNEL);
	if(! *req_entry) {
		pr_err("out of memory\n");

		err = ENOMEM;
		goto err0;
	}
	INIT_LIST_HEAD(&(*req_entry)->node);

	return 0;

err0:
	return err;
}

int qvio_umods_rsp_entry_new(struct qvio_umods_rsp_entry** rsp_entry) {
	int err;

	*rsp_entry = kzalloc(sizeof(struct qvio_umods_rsp_entry), GFP_KERNEL);
	if(! *rsp_entry) {
		pr_err("out of memory\n");

		err = ENOMEM;
		goto err0;
	}
	INIT_LIST_HEAD(&(*rsp_entry)->node);

	return 0;

err0:
	return err;
}

static int __file_release(struct inode *inode, struct file *filep) {
	struct qvio_umods* self = filep->private_data;

	pr_info("self=%p\n", self);

	return 0;
}

static __poll_t __file_poll(struct file *filep, struct poll_table_struct *wait) {
	struct qvio_umods* self = filep->private_data;

	poll_wait(filep, &self->req_wq, wait);

	if(!list_empty(&self->req_list))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static long __ioctl_g_umods_req(struct file *filep, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_umods* self = filep->private_data;
	struct qvio_umods_req_entry *req_entry;
	unsigned long flags;

	if(list_empty(&self->req_list)) {
		pr_err("unexpected value, self->req_list is empty\n");

		ret = -EFAULT;
		goto err0;
	}

	spin_lock_irqsave(&self->req_list_lock, flags);
	req_entry = list_first_entry(&self->req_list, struct qvio_umods_req_entry, node);
	list_del(&req_entry->node);
	spin_unlock_irqrestore(&self->req_list_lock, flags);

#if 0 // DEBUG
	pr_info("-req(%d, %d)\n",
		(int)req_entry->req.job_id,
		(int)req_entry->req.sequence);
#endif

	ret = copy_to_user((void __user *)arg, &req_entry->req, sizeof(struct qvio_umods_req));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err1;
	}

	kfree(req_entry);
	ret = 0;

	return ret;

err1:
	kfree(req_entry);
err0:
	return ret;
};

static long __ioctl_s_umods_rsp(struct file *filep, unsigned int cmd, unsigned long arg) {
	long ret;
	int err;
	struct qvio_umods* self = filep->private_data;
	struct qvio_umods_rsp_entry *rsp_entry;
	unsigned long flags;

#if 0 // DEBUG
	pr_info("\n");
#endif

	err = qvio_umods_rsp_entry_new(&rsp_entry);
	if(err) {
		pr_err("qvio_umods_rsp_entry_new() failed, err=%d\n", err);

		ret = -err;
		goto err0;
	}

	ret = copy_from_user(&rsp_entry->rsp, (void __user *)arg, sizeof(struct qvio_umods_rsp));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err1;
	}

#if 0 // DEBUG
	pr_info("+rsp(%d, %d)\n",
		(int)rsp_entry->rsp.job_id,
		(int)rsp_entry->rsp.sequence);
#endif

	spin_lock_irqsave(&self->rsp_list_lock, flags);
	list_add_tail(&rsp_entry->node, &self->rsp_list);
	spin_unlock_irqrestore(&self->rsp_list_lock, flags);

	wake_up_interruptible(&self->rsp_wq);

	ret = 0;

	return ret;

err1:
	kfree(rsp_entry);
err0:
	return ret;
}

static long __file_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
	switch (cmd) {
	case QVID_IOC_G_UMODS_REQ:
		return __ioctl_g_umods_req(filep, cmd, arg);
		break;

	case QVID_IOC_S_UMODS_RSP:
		return __ioctl_s_umods_rsp(filep, cmd, arg);
		break;

	default:
		return -EINVAL;
		break;
	}
}

#ifdef CONFIG_COMPAT
static long __file_ioctl_compat(struct file *filep, unsigned int cmd,
				   unsigned long arg)
{
	return __file_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations __fileops = {
	.owner = THIS_MODULE,
	.release = __file_release,
	.poll = __file_poll,
	.llseek = noop_llseek,
	.unlocked_ioctl = __file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = __file_ioctl_compat,
#endif
};

void qvio_umods_start(struct qvio_umods* self) {
	atomic_set(&self->sequence, 0);

	init_waitqueue_head(&self->req_wq);
	spin_lock_init(&self->req_list_lock);
	INIT_LIST_HEAD(&self->req_list);

	init_waitqueue_head(&self->rsp_wq);
	spin_lock_init(&self->rsp_list_lock);
	INIT_LIST_HEAD(&self->rsp_list);
}

void qvio_umods_stop(struct qvio_umods* self) {
	struct qvio_umods_req_entry *req_entry;
	struct qvio_umods_rsp_entry *rsp_entry;

	pr_info("\n");

	if(! list_empty(&self->req_list)) {
		pr_warn("self->req_list is not empty\n");

#if 1
		list_for_each_entry(req_entry, &self->req_list, node) {
			list_del(&req_entry->node);

			pr_warn("req_entry->req={%d %d}\n",
				(int)req_entry->req.job_id,
				(int)req_entry->req.sequence);

			kfree(req_entry);
		}
#endif
	}

	if(! list_empty(&self->rsp_list)) {
		pr_warn("self->rsp_list is not empty\n");

#if 1
		list_for_each_entry(rsp_entry, &self->rsp_list, node) {
			list_del(&rsp_entry->node);

			pr_warn("rsp_entry->rsp={%d %d}\n",
				(int)rsp_entry->rsp.job_id,
				(int)rsp_entry->rsp.sequence);

			kfree(rsp_entry);
		}
#endif
	}
}

int qvio_umods_get_fd(struct qvio_umods* self, const char* name, int flags) {
	int err;
	int fd;
	struct file* file;

	fd = get_unused_fd_flags(flags);
	if (fd < 0) {
		pr_err("get_unused_fd_flags() failed, fd=%d\n", fd);

		err = fd;
		goto err0;
	}

	file = anon_inode_getfile(name, &__fileops, self, flags);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		pr_err("anon_inode_getfile() failed, err=%d\n", err);

		goto err1;
	}

	fd_install(fd, file);
	err = fd;

	return err;

err1:
	put_unused_fd(fd);
err0:
	return err;
}

void qvio_umods_request(struct qvio_umods* self, struct qvio_umods_req_entry* req_entry) {
	unsigned long flags;

	req_entry->req.sequence = (__u16)atomic_inc_return(&self->sequence);

#if 1 // DEBUG
	pr_info("+req(%d, %d)\n",
		(int)req_entry->req.job_id,
		(int)req_entry->req.sequence);
#endif

	spin_lock_irqsave(&self->req_list_lock, flags);
	list_add_tail(&req_entry->node, &self->req_list);
	spin_unlock_irqrestore(&self->req_list_lock, flags);

	wake_up_interruptible(&self->req_wq);
}

int qvio_umods_response(struct qvio_umods* self, struct qvio_umods_rsp_entry** rsp_entry) {
	int err;
	unsigned long flags;

#if 0 // DEBUG
	pr_info("\n");
#endif

	// wait for user-job-done
	err = wait_event_interruptible(self->rsp_wq, !list_empty(&self->rsp_list));
	if(err != 0) {
		pr_err("wait_event_interruptible() failed, err=%d\n", err);
		goto err0;
	}

#if 0 // DEBUG
	pr_info("\n");
#endif

	if(list_empty(&self->rsp_list)) {
		pr_err("self->rsp_list is empty\n");
		err = EFAULT;
		goto err0;
	}

	spin_lock_irqsave(&self->rsp_list_lock, flags);
	*rsp_entry = list_first_entry(&self->rsp_list, struct qvio_umods_rsp_entry, node);
	list_del(&(*rsp_entry)->node);
	spin_unlock_irqrestore(&self->rsp_list_lock, flags);

#if 1 // DEBUG
	pr_info("-rsp(%d, %d)\n",
		(int)(*rsp_entry)->rsp.job_id,
		(int)(*rsp_entry)->rsp.sequence);
#endif

	return 0;

err0:
	return err;
}
