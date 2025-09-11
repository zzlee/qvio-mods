#ifndef __QVIO_PCI_DEVICE_E382_H__
#define __QVIO_PCI_DEVICE_E382_H__

#include "pci_device.h"

int device_e382_probe(struct qvio_pci_device* self);
void device_e382_remove(struct qvio_pci_device* self);
int device_e382_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev);
void device_e382_free_irqs(struct qvio_pci_device* self, struct pci_dev* pdev);

#endif // __QVIO_PCI_DEVICE_E382_H__