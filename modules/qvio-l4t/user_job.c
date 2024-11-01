#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "user_job.h"

#include <linux/compat.h>

struct __user_job_entry {
	struct list_head node;
	struct qvio_user_job user_job;
};

struct __user_job_done_entry {
	struct list_head node;
	struct qvio_user_job_done user_job_done;
};

int __user_job_entry_new(struct __user_job_entry** user_job_entry) {
	int err;

	*user_job_entry = kzalloc(sizeof(struct __user_job_entry), GFP_KERNEL);
	if(! *user_job_entry) {
		pr_err("out of memory\n");

		err = ENOMEM;
		goto err0;
	}
	INIT_LIST_HEAD(&(*user_job_entry)->node);

	return 0;

err0:
	return err;
}

int __user_job_done_entry_new(struct __user_job_done_entry** user_job_done_entry) {
	int err;

	*user_job_done_entry = kzalloc(sizeof(struct __user_job_done_entry), GFP_KERNEL);
	if(! *user_job_done_entry) {
		pr_err("out of memory\n");

		err = ENOMEM;
		goto err0;
	}
	INIT_LIST_HEAD(&(*user_job_done_entry)->node);

	return 0;

err0:
	return err;
}

static int __file_release(struct inode *inode, struct file *filep) {
	struct qvio_user_job_ctrl* self = filep->private_data;

	pr_info("self=%p\n", self);

	return 0;
}

static __poll_t __file_poll(struct file *filep, struct poll_table_struct *wait) {
	struct qvio_user_job_ctrl* self = filep->private_data;

	poll_wait(filep, &self->job_wq, wait);

	if(!list_empty(&self->job_list))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static long __ioctl_user_job_get(struct qvio_user_job_ctrl* self, unsigned long arg) {
	long ret;
	struct __user_job_entry *user_job_entry;
	unsigned long flags;

	if(list_empty(&self->job_list)) {
		pr_err("unexpected value, self->job_list is empty\n");

		ret = -EFAULT;
		goto err0;
	}

	spin_lock_irqsave(&self->job_list_lock, flags);
	user_job_entry = list_first_entry(&self->job_list, struct __user_job_entry, node);
	list_del(&user_job_entry->node);
	spin_unlock_irqrestore(&self->job_list_lock, flags);

#if 0 // DEBUG
	pr_info("-user_job(%d, %d)\n",
		(int)user_job_entry->user_job.id,
		(int)user_job_entry->user_job.sequence);
#endif

	ret = copy_to_user((void __user *)arg, &user_job_entry->user_job, sizeof(struct qvio_user_job));
	if (ret != 0) {
		pr_err("copy_to_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err1;
	}

	kfree(user_job_entry);
	ret = 0;

	return ret;

err1:
	kfree(user_job_entry);
err0:
	return ret;
}

static long __ioctl_user_job_done(struct qvio_user_job_ctrl* self, unsigned long arg) {
	int err;
	long ret;
	struct __user_job_done_entry *user_job_done_entry;
	unsigned long flags;

#if 0 // DEBUG
	pr_info("\n");
#endif

	err = __user_job_done_entry_new(&user_job_done_entry);
	if(err) {
		pr_err("__user_job_done_entry_new() failed, err=%d\n", err);

		ret = -err;
		goto err0;
	}

	ret = copy_from_user(&user_job_done_entry->user_job_done, (void __user *)arg, sizeof(struct qvio_user_job_done));
	if (ret != 0) {
		pr_err("copy_from_user() failed, err=%d\n", (int)ret);

		ret = -EFAULT;
		goto err1;
	}

#if 0 // DEBUG
	pr_info("+user_job_done(%d, %d)\n",
		(int)user_job_done_entry->user_job_done.id,
		(int)user_job_done_entry->user_job_done.sequence);
#endif

	spin_lock_irqsave(&self->done_list_lock, flags);
	list_add_tail(&user_job_done_entry->node, &self->done_list);
	spin_unlock_irqrestore(&self->done_list_lock, flags);

	wake_up_interruptible(&self->done_wq);

	ret = 0;

	return ret;

err1:
	kfree(user_job_done_entry);
err0:
	return ret;
}

static long __file_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
	long ret;
	struct qvio_user_job_ctrl* self = filep->private_data;

	switch (cmd) {
	case QVID_IOC_USER_JOB_GET:
		ret = __ioctl_user_job_get(self, arg);
		break;

	case QVID_IOC_USER_JOB_DONE:
		ret = __ioctl_user_job_done(self, arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
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

void qvio_user_job_start(struct qvio_user_job_ctrl* self) {
	pr_info("\n");

	self->enable = true;
	atomic_set(&self->sequence, 0);

	init_waitqueue_head(&self->job_wq);
	spin_lock_init(&self->job_list_lock);
	INIT_LIST_HEAD(&self->job_list);

	init_waitqueue_head(&self->done_wq);
	spin_lock_init(&self->done_list_lock);
	INIT_LIST_HEAD(&self->done_list);

	self->ctrl_fops = &__fileops;
}

void qvio_user_job_stop(struct qvio_user_job_ctrl* self) {
	struct __user_job_entry *user_job_entry;
	struct __user_job_done_entry *user_job_done_entry;

	pr_info("\n");

	if(! list_empty(&self->job_list)) {
		pr_warn("self->job_list is not empty\n");

#if 1
		list_for_each_entry(user_job_entry, &self->job_list, node) {
			list_del(&user_job_entry->node);

			pr_warn("user_job_entry->user_job={%d %d}\n",
				(int)user_job_entry->user_job.id,
				(int)user_job_entry->user_job.sequence);

			kfree(user_job_entry);
		}
#endif
	}

	if(! list_empty(&self->done_list)) {
		pr_warn("self->done_list is not empty\n");

#if 1
		list_for_each_entry(user_job_done_entry, &self->done_list, node) {
			list_del(&user_job_done_entry->node);

			pr_warn("user_job_done_entry->user_job_done={%d %d}\n",
				(int)user_job_done_entry->user_job_done.id,
				(int)user_job_done_entry->user_job_done.sequence);

			kfree(user_job_done_entry);
		}
#endif
	}
}

static void __do_user_job(struct qvio_user_job_ctrl* self, struct __user_job_entry* user_job_entry) {
	unsigned long flags;

#if 0 // DEBUG
	pr_info("+user_job(%d, %d)\n",
		(int)user_job_entry->user_job.id,
		(int)user_job_entry->user_job.sequence);
#endif

	spin_lock_irqsave(&self->job_list_lock, flags);
	list_add_tail(&user_job_entry->node, &self->job_list);
	spin_unlock_irqrestore(&self->job_list_lock, flags);

	wake_up_interruptible(&self->job_wq);
}

static int __wait_for_user_job_done(struct qvio_user_job_ctrl* self, void* user, qvio_user_job_done_handler fn) {
	int err;
	unsigned long flags;
	struct __user_job_done_entry* user_job_done_entry;

#if 0 // DEBUG
	pr_info("\n");
#endif

	// wait for user-job-done
	err = wait_event_interruptible(self->done_wq, !list_empty(&self->done_list));
	if(err != 0) {
		pr_err("wait_event_interruptible() failed, err=%d\n", err);
		goto err0;
	}

#if 0 // DEBUG
	pr_info("\n");
#endif

	if(list_empty(&self->done_list)) {
		pr_err("self->done_list is empty\n");
		err = EFAULT;
		goto err0;
	}

	spin_lock_irqsave(&self->done_list_lock, flags);
	user_job_done_entry = list_first_entry(&self->done_list, struct __user_job_done_entry, node);
	list_del(&user_job_done_entry->node);
	spin_unlock_irqrestore(&self->done_list_lock, flags);

#if 0 // DEBUG
	pr_info("-user_job_done(%d, %d)\n",
		(int)user_job_done_entry->user_job_done.id,
		(int)user_job_done_entry->user_job_done.sequence);
#endif

	if(fn) {
		fn(user, &user_job_done_entry->user_job_done);
	}
	kfree(user_job_done_entry);

	return 0;

err0:
	return err;
}

int qvio_user_job_s_fmt(struct qvio_user_job_ctrl* self, struct v4l2_format *format) {
	int err;
	struct __user_job_entry *user_job_entry;

	pr_info("enable=%d\n", (int)self->enable);

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_S_FMT;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	memcpy(&user_job_entry->user_job.u.s_fmt.format, format, sizeof(struct v4l2_format));
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_queue_setup(struct qvio_user_job_ctrl* self, unsigned int num_buffers) {
	int err;
	struct __user_job_entry *user_job_entry;

	pr_info("enable=%d\n", (int)self->enable);

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_QUEUE_SETUP;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.queue_setup.num_buffers = num_buffers;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_buf_init(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer, void* user, qvio_user_job_done_handler fn) {
	int err;
	struct __user_job_entry *user_job_entry;

	pr_info("enable=%d\n", (int)self->enable);

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_BUF_INIT;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.buf_init.index = buffer->index;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, user, fn);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_buf_cleanup(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer) {
	int err;
	struct __user_job_entry *user_job_entry;

#if 0 // DEBUG
	pr_info("enable=%d\n", (int)self->enable);
#endif

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_BUF_CLEANUP;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.buf_cleanup.index = buffer->index;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_start_streaming(struct qvio_user_job_ctrl* self) {
	int err;
	struct __user_job_entry *user_job_entry;

	pr_info("enable=%d\n", (int)self->enable);

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_START_STREAMING;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.start_streaming.flags = 0;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_stop_streaming(struct qvio_user_job_ctrl* self) {
	int err;
	struct __user_job_entry *user_job_entry;

	pr_info("enable=%d\n", (int)self->enable);

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_STOP_STREAMING;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.stop_streaming.flags = 0;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}

int qvio_user_job_buf_done(struct qvio_user_job_ctrl* self, struct vb2_buffer *buffer) {
	int err;
	struct __user_job_entry *user_job_entry;

#if 0 // DEBUG
	pr_info("enable=%d\n", (int)self->enable);
#endif

	if(! self->enable)
		return 0;

	err = __user_job_entry_new(&user_job_entry);
	if(err) {
		pr_err("__user_job_entry_new() failed, err=%d\n", err);
		goto err0;
	}

	user_job_entry->user_job.id = QVIO_USER_JOB_ID_BUF_DONE;
	user_job_entry->user_job.sequence = (__u16)atomic_inc_return(&self->sequence);
	user_job_entry->user_job.u.buf_done.index = buffer->index;
	__do_user_job(self, user_job_entry);

	err = __wait_for_user_job_done(self, NULL, NULL);
	if(err) {
		pr_err("__wait_for_user_job_done() failed, err=%d\n", err);
		goto err0;
	}

	return 0;

err0:
	return err;
}
