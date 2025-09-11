#ifndef __QVIO_PCI_DEVICE_7024_H__
#define __QVIO_PCI_DEVICE_7024_H__

#include "pci_device.h"

int device_7024_probe(struct qvio_pci_device* self);
void device_7024_remove(struct qvio_pci_device* self);
int device_7024_irq_setup(struct qvio_pci_device* self, struct pci_dev* pdev);
void device_7024_free_irqs(struct qvio_pci_device* self, struct pci_dev* pdev);

#endif // __QVIO_PCI_DEVICE_7024_H__