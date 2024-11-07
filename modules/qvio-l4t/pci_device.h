#ifndef __QVIO_PCI_DEVICE_H__
#define __QVIO_PCI_DEVICE_H__

#if defined(RHEL_RELEASE_CODE)
#	define PCI_AER_NAMECHANGE (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3))
#else
#	define PCI_AER_NAMECHANGE (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
#endif

int qvio_device_pci_register(void);
void qvio_device_pci_unregister(void);

#endif // __QVIO_PCI_DEVICE_H__