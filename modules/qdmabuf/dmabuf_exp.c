#define pr_fmt(fmt)     "[" KBUILD_MODNAME "]%s(#%d): " fmt, __func__, __LINE__

#include "dmabuf_exp.h"

static void dmabuf_exp_vm_open(struct vm_area_struct *vma)
{
	struct dmabuf_exp_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%p, refcount: %d, vma: %08lx-%08lx\n",
		h, refcount_read(h->refcount), vma->vm_start,
		vma->vm_end);

	refcount_inc(h->refcount);
}

static void dmabuf_exp_vm_close(struct vm_area_struct *vma)
{
	struct dmabuf_exp_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%p, refcount: %d, vma: %08lx-%08lx\n",
		h, refcount_read(h->refcount), vma->vm_start,
		vma->vm_end);

	h->put(h->arg);
}

const struct vm_operations_struct dmabuf_exp_vm_ops = {
	.open = dmabuf_exp_vm_open,
	.close = dmabuf_exp_vm_close,
};
