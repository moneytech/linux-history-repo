/*
 * File:	portdrv_core.c
 * Purpose:	PCI Express Port Bus Driver's Core Functions
 *
 * Copyright (C) 2004 Intel
 * Copyright (C) Tom Long Nguyen (tom.l.nguyen@intel.com)
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/pcieport_if.h>

#include "../pci.h"
#include "portdrv.h"

/**
 * release_pcie_device - free PCI Express port service device structure
 * @dev: Port service device to release
 *
 * Invoked automatically when device is being removed in response to
 * device_unregister(dev).  Release all resources being claimed.
 */
static void release_pcie_device(struct device *dev)
{
	kfree(to_pcie_device(dev));			
}

/**
 * assign_interrupt_mode - choose interrupt mode for PCI Express port services
 *                         (INTx, MSI-X, MSI) and set up vectors
 * @dev: PCI Express port to handle
 * @vectors: Array of interrupt vectors to populate
 * @mask: Bitmask of port capabilities returned by get_port_device_capability()
 *
 * Return value: Interrupt mode associated with the port
 */
static int assign_interrupt_mode(struct pci_dev *dev, int *vectors, int mask)
{
	struct pcie_port_data *port_data = pci_get_drvdata(dev);
	int i, pos, nvec, status = -EINVAL;
	int interrupt_mode = PCIE_PORT_NO_IRQ;

	/* Set INTx as default */
	for (i = 0, nvec = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++) {
		if (mask & (1 << i)) 
			nvec++;
		vectors[i] = dev->irq;
	}
	if (dev->pin)
		interrupt_mode = PCIE_PORT_INTx_MODE;

	/* Check MSI quirk */
	if (port_data->port_type == PCIE_RC_PORT && pcie_mch_quirk)
		return interrupt_mode;

	/* Select MSI-X over MSI if supported */		
	pos = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (pos) {
		struct msix_entry msix_entries[PCIE_PORT_DEVICE_MAXSERVICES] = 
			{{0, 0}, {0, 1}, {0, 2}, {0, 3}};
		status = pci_enable_msix(dev, msix_entries, nvec);
		if (!status) {
			int j = 0;

			interrupt_mode = PCIE_PORT_MSIX_MODE;
			for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++) {
				if (mask & (1 << i)) 
					vectors[i] = msix_entries[j++].vector;
			}
		}
	} 
	if (status) {
		pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
		if (pos) {
			status = pci_enable_msi(dev);
			if (!status) {
				interrupt_mode = PCIE_PORT_MSI_MODE;
				for (i = 0;i < PCIE_PORT_DEVICE_MAXSERVICES;i++)
					vectors[i] = dev->irq;
			}
		}
	} 
	return interrupt_mode;
}

/**
 * get_port_device_capability - discover capabilities of a PCI Express port
 * @dev: PCI Express port to examine
 *
 * The capabilities are read from the port's PCI Express configuration registers
 * as described in PCI Express Base Specification 1.0a sections 7.8.2, 7.8.9 and
 * 7.9 - 7.11.
 *
 * Return value: Bitmask of discovered port capabilities
 */
static int get_port_device_capability(struct pci_dev *dev)
{
	int services = 0, pos;
	u16 reg16;
	u32 reg32;

	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	pci_read_config_word(dev, pos + PCIE_CAPABILITIES_REG, &reg16);
	/* Hot-Plug Capable */
	if (reg16 & PORT_TO_SLOT_MASK) {
		pci_read_config_dword(dev, 
			pos + PCIE_SLOT_CAPABILITIES_REG, &reg32);
		if (reg32 & SLOT_HP_CAPABLE_MASK)
			services |= PCIE_PORT_SERVICE_HP;
	}
	/* AER capable */
	if (pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR))
		services |= PCIE_PORT_SERVICE_AER;
	/* VC support */
	if (pci_find_ext_capability(dev, PCI_EXT_CAP_ID_VC))
		services |= PCIE_PORT_SERVICE_VC;

	return services;
}

/**
 * pcie_device_init - initialize PCI Express port service device
 * @dev: Port service device to initialize
 * @parent: PCI Express port to associate the service device with
 * @port_type: Type of the port
 * @service_type: Type of service to associate with the service device
 * @irq: Interrupt vector to associate with the service device
 */
static void pcie_device_init(struct pci_dev *parent, struct pcie_device *dev, 
	int service_type, int irq)
{
	struct pcie_port_data *port_data = pci_get_drvdata(parent);
	struct device *device;
	int port_type = port_data->port_type;

	dev->port = parent;
	dev->irq = irq;
	dev->service = service_type;

	/* Initialize generic device interface */
	device = &dev->device;
	memset(device, 0, sizeof(struct device));
	device->bus = &pcie_port_bus_type;
	device->driver = NULL;
	device->driver_data = NULL;
	device->release = release_pcie_device;	/* callback to free pcie dev */
	dev_set_name(device, "%s:pcie%02x",
		 pci_name(parent), get_descriptor_id(port_type, service_type));
	device->parent = &parent->dev;
}

/**
 * alloc_pcie_device - allocate PCI Express port service device structure
 * @parent: PCI Express port to associate the service device with
 * @port_type: Type of the port
 * @service_type: Type of service to associate with the service device
 * @irq: Interrupt vector to associate with the service device
 */
static struct pcie_device* alloc_pcie_device(struct pci_dev *parent,
	int service_type, int irq)
{
	struct pcie_device *device;

	device = kzalloc(sizeof(struct pcie_device), GFP_KERNEL);
	if (!device)
		return NULL;

	pcie_device_init(parent, device, service_type, irq);
	return device;
}

/**
 * pcie_port_device_probe - check if device is a PCI Express port
 * @dev: Device to check
 */
int pcie_port_device_probe(struct pci_dev *dev)
{
	int pos, type;
	u16 reg;

	if (!(pos = pci_find_capability(dev, PCI_CAP_ID_EXP)))
		return -ENODEV;

	pci_read_config_word(dev, pos + PCIE_CAPABILITIES_REG, &reg);
	type = (reg >> 4) & PORT_TYPE_MASK;
	if (	type == PCIE_RC_PORT || type == PCIE_SW_UPSTREAM_PORT ||
		type == PCIE_SW_DOWNSTREAM_PORT )
		return 0;

	return -ENODEV;
}

/**
 * pcie_port_device_register - register PCI Express port
 * @dev: PCI Express port to register
 *
 * Allocate the port extension structure and register services associated with
 * the port.
 */
int pcie_port_device_register(struct pci_dev *dev)
{
	struct pcie_port_data *port_data;
	int status, capabilities, irq_mode, i, nr_serv;
	int vectors[PCIE_PORT_DEVICE_MAXSERVICES];
	u16 reg16;

	port_data = kzalloc(sizeof(*port_data), GFP_KERNEL);
	if (!port_data)
		return -ENOMEM;
	pci_set_drvdata(dev, port_data);

	/* Get port type */
	pci_read_config_word(dev,
		pci_find_capability(dev, PCI_CAP_ID_EXP) +
		PCIE_CAPABILITIES_REG, &reg16);
	port_data->port_type = (reg16 >> 4) & PORT_TYPE_MASK;

	capabilities = get_port_device_capability(dev);
	/* Root ports are capable of generating PME too */
	if (port_data->port_type == PCIE_RC_PORT)
		capabilities |= PCIE_PORT_SERVICE_PME;

	irq_mode = assign_interrupt_mode(dev, vectors, capabilities);
	if (irq_mode == PCIE_PORT_NO_IRQ) {
		/*
		 * Don't use service devices that require interrupts if there is
		 * no way to generate them.
		 */
		if (!(capabilities & PCIE_PORT_SERVICE_VC)) {
			status = -ENODEV;
			goto Error;
		}
		capabilities = PCIE_PORT_SERVICE_VC;
	}
	port_data->port_irq_mode = irq_mode;

	status = pci_enable_device(dev);
	if (status)
		goto Error;
	pci_set_master(dev);

	/* Allocate child services if any */
	for (i = 0, nr_serv = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++) {
		struct pcie_device *child;
		int service = 1 << i;

		if (!(capabilities & service))
			continue;

		child = alloc_pcie_device(dev, service, vectors[i]);
		if (!child)
			continue;

		status = device_register(&child->device);
		if (status) {
			kfree(child);
			continue;
		}

		get_device(&child->device);
		nr_serv++;
	}
	if (!nr_serv) {
		pci_disable_device(dev);
		status = -ENODEV;
		goto Error;
	}

	return 0;

 Error:
	kfree(port_data);
	return status;
}

#ifdef CONFIG_PM
static int suspend_iter(struct device *dev, void *data)
{
	struct pcie_port_service_driver *service_driver;
	pm_message_t state = * (pm_message_t *) data;

 	if ((dev->bus == &pcie_port_bus_type) &&
 	    (dev->driver)) {
 		service_driver = to_service_driver(dev->driver);
 		if (service_driver->suspend)
 			service_driver->suspend(to_pcie_device(dev), state);
  	}
	return 0;
}

/**
 * pcie_port_device_suspend - suspend port services associated with a PCIe port
 * @dev: PCI Express port to handle
 * @state: Representation of system power management transition in progress
 */
int pcie_port_device_suspend(struct pci_dev *dev, pm_message_t state)
{
	return device_for_each_child(&dev->dev, &state, suspend_iter);
}

static int resume_iter(struct device *dev, void *data)
{
	struct pcie_port_service_driver *service_driver;

	if ((dev->bus == &pcie_port_bus_type) &&
	    (dev->driver)) {
		service_driver = to_service_driver(dev->driver);
		if (service_driver->resume)
			service_driver->resume(to_pcie_device(dev));
	}
	return 0;
}

/**
 * pcie_port_device_suspend - resume port services associated with a PCIe port
 * @dev: PCI Express port to handle
 */
int pcie_port_device_resume(struct pci_dev *dev)
{
	return device_for_each_child(&dev->dev, NULL, resume_iter);
}
#endif

static int remove_iter(struct device *dev, void *data)
{
	struct pcie_port_service_driver *service_driver;

	if (dev->bus == &pcie_port_bus_type) {
		if (dev->driver) {
			service_driver = to_service_driver(dev->driver);
			if (service_driver->remove)
				service_driver->remove(to_pcie_device(dev));
		}
		*(unsigned long*)data = (unsigned long)dev;
		return 1;
	}
	return 0;
}

/**
 * pcie_port_device_remove - unregister PCI Express port service devices
 * @dev: PCI Express port the service devices to unregister are associated with
 *
 * Remove PCI Express port service devices associated with given port and
 * disable MSI-X or MSI for the port.
 */
void pcie_port_device_remove(struct pci_dev *dev)
{
	struct pcie_port_data *port_data = pci_get_drvdata(dev);
	int status;

	do {
		unsigned long device_addr;

		status = device_for_each_child(&dev->dev, &device_addr, remove_iter);
		if (status) {
			struct device *device = (struct device*)device_addr;
			put_device(device);
			device_unregister(device);
		}
	} while (status);

	switch (port_data->port_irq_mode) {
	case PCIE_PORT_MSIX_MODE:
		pci_disable_msix(dev);
		break;
	case PCIE_PORT_MSI_MODE:
		pci_disable_msi(dev);
		break;
	}

	kfree(port_data);
}

/**
 * pcie_port_probe_service - probe driver for given PCI Express port service
 * @dev: PCI Express port service device to probe against
 *
 * If PCI Express port service driver is registered with
 * pcie_port_service_register(), this function will be called by the driver core
 * whenever match is found between the driver and a port service device.
 */
static int pcie_port_probe_service(struct device *dev)
{
	struct pcie_device *pciedev;
	struct pcie_port_service_driver *driver;
	int status;

	if (!dev || !dev->driver)
		return -ENODEV;

	driver = to_service_driver(dev->driver);
	if (!driver || !driver->probe)
		return -ENODEV;

	pciedev = to_pcie_device(dev);
	status = driver->probe(pciedev);
	if (!status) {
		dev_printk(KERN_DEBUG, dev, "service driver %s loaded\n",
			driver->name);
		get_device(dev);
	}
	return status;
}

/**
 * pcie_port_remove_service - detach driver from given PCI Express port service
 * @dev: PCI Express port service device to handle
 *
 * If PCI Express port service driver is registered with
 * pcie_port_service_register(), this function will be called by the driver core
 * when device_unregister() is called for the port service device associated
 * with the driver.
 */
static int pcie_port_remove_service(struct device *dev)
{
	struct pcie_device *pciedev;
	struct pcie_port_service_driver *driver;

	if (!dev || !dev->driver)
		return 0;

	pciedev = to_pcie_device(dev);
	driver = to_service_driver(dev->driver);
	if (driver && driver->remove) {
		dev_printk(KERN_DEBUG, dev, "unloading service driver %s\n",
			driver->name);
		driver->remove(pciedev);
		put_device(dev);
	}
	return 0;
}

/**
 * pcie_port_shutdown_service - shut down given PCI Express port service
 * @dev: PCI Express port service device to handle
 *
 * If PCI Express port service driver is registered with
 * pcie_port_service_register(), this function will be called by the driver core
 * when device_shutdown() is called for the port service device associated
 * with the driver.
 */
static void pcie_port_shutdown_service(struct device *dev) {}

/**
 * pcie_port_service_register - register PCI Express port service driver
 * @new: PCI Express port service driver to register
 */
int pcie_port_service_register(struct pcie_port_service_driver *new)
{
	new->driver.name = (char *)new->name;
	new->driver.bus = &pcie_port_bus_type;
	new->driver.probe = pcie_port_probe_service;
	new->driver.remove = pcie_port_remove_service;
	new->driver.shutdown = pcie_port_shutdown_service;

	return driver_register(&new->driver);
}

/**
 * pcie_port_service_unregister - unregister PCI Express port service driver
 * @drv: PCI Express port service driver to unregister
 */
void pcie_port_service_unregister(struct pcie_port_service_driver *drv)
{
	driver_unregister(&drv->driver);
}

EXPORT_SYMBOL(pcie_port_service_register);
EXPORT_SYMBOL(pcie_port_service_unregister);
