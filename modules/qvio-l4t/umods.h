#ifndef __QVIO_UMODS_H__
#define __QVIO_UMODS_H__

#include "uapi/qvio-l4t.h"

#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>

struct qvio_umods;

struct qvio_umods_req_entry {
	struct list_head node;
	struct qvio_umods_req req;
};

struct qvio_umods_rsp_entry {
	struct list_head node;
	struct qvio_umods_rsp rsp;
};

struct qvio_umods {
	atomic_t sequence;

	// req list
	wait_queue_head_t req_wq;
	spinlock_t req_list_lock;
	struct list_head req_list;

	// rsp list
	wait_queue_head_t rsp_wq;
	spinlock_t rsp_list_lock;
	struct list_head rsp_list;
};

int qvio_umods_req_entry_new(struct qvio_umods_req_entry** req_entry);
int qvio_umods_rsp_entry_new(struct qvio_umods_rsp_entry** rsp_entry);

void qvio_umods_start(struct qvio_umods* self);
void qvio_umods_stop(struct qvio_umods* self);
int qvio_umods_get_fd(struct qvio_umods* self, const char* name, int flags); // flags = O_RDONLY | O_CLOEXEC
void qvio_umods_request(struct qvio_umods* self, struct qvio_umods_req_entry* req_entry);
int qvio_umods_response(struct qvio_umods* self, struct qvio_umods_rsp_entry** rsp_entry);

#endif // __QVIO_UMODS_H__