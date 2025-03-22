#ifndef __QVIO_PCI_DEVICE_H__
#define __QVIO_PCI_DEVICE_H__

#include <linux/types.h>

#if defined(RHEL_RELEASE_CODE)
#	define PCI_AER_NAMECHANGE (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3))
#else
#	define PCI_AER_NAMECHANGE (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
#endif

#define DESC_BITSHIFT				4
#define DESC_SIZE					(1 << DESC_BITSHIFT)

struct __attribute__((__packed__)) desc_item_t {
	s32 nOffsetHigh;
	u32 nOffsetLow;
	u32 nSize;
	u32 nFlags;
};

int qvio_device_pci_register(void);
void qvio_device_pci_unregister(void);

#endif // __QVIO_PCI_DEVICE_H__