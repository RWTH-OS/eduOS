/** 
 * @author Stefan Lankes
 * @file arch/x86/include/asm/pci.h
 * @brief functions related to PCI initialization and information
 *
 * This file contains a procedure to initialize the PCI environment
 * and functions to access information about specific PCI devices.
 */

#ifndef __ARCH_PCI_H__
#define __ARCH_PCI_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t base[6];
	uint32_t size[6];
	uint32_t irq;
} pci_info_t;

/** @brief Initialize the PCI environment
 */
int pci_init(void);

/** @brief Determine the IObase address and the interrupt number of a specific device
 *
 * @param vendor_id The device's vendor ID
 * @param device_id the device's ID
 * @param info Pointer to the record pci_info_t where among other the IObase address will be stored
 *
 * @return 
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int pci_get_device_info(uint32_t vendor_id, uint32_t device_id, pci_info_t* info);

/** @brief Print information of existing pci adapters
 *
 * @return 0 in any case
 */
int print_pci_adapters(void);

#ifdef __cplusplus
}
#endif

#endif
