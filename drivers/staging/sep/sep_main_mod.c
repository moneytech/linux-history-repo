/*
 *
 *  sep_main_mod.c - Security Processor Driver main group of functions
 *
 *  Copyright(c) 2009 Intel Corporation. All rights reserved.
 *  Copyright(c) 2009 Discretix. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  CONTACTS:
 *
 *  Mark Allyn		mark.a.allyn@intel.com
 *
 *  CHANGES:
 *
 *  2009.06.26	Initial publish
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/ioctl.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <asm/cacheflush.h>
#include "sep_driver_hw_defs.h"
#include "sep_driver_config.h"
#include "sep_driver_api.h"
#include "sep_driver_ext_api.h"
#include "sep_dev.h"

/*----------------------------------------
	DEFINES
-----------------------------------------*/


#define INT_MODULE_PARM(n, v) int n = v; module_param(n, int, 0)

/*--------------------------------------
  TYPEDEFS
  -----------------------------------------*/



/*--------------------------------------------
	GLOBAL variables
--------------------------------------------*/

/* debug messages level */
INT_MODULE_PARM(sepDebug, 0x0);
MODULE_PARM_DESC(sepDebug, "Flag to enable SEP debug messages");


/*
  mutex for the access to the internals of the sep driver
*/
static DEFINE_MUTEX(sep_mutex);


/* wait queue head (event) of the driver */
static DECLARE_WAIT_QUEUE_HEAD(g_sep_event);



/*------------------------------------------------
  PROTOTYPES
---------------------------------------------------*/

/*
  interrupt handler function
*/
irqreturn_t sep_inthandler(int irq, void *dev_id);

/*
  this function registers the driver to the file system
*/
static int sep_register_driver_to_fs(void);

/*
  this function unregisters driver from fs
*/
static void sep_unregister_driver_from_fs(void);

/*
  this function calculates the size of data that can be inserted into the lli
  table from this array the condition is that either the table is full
  (all etnries are entered), or there are no more entries in the lli array
*/
static unsigned long sep_calculate_lli_table_max_size(struct sep_lli_entry_t *lli_in_array_ptr, unsigned long num_array_entries);
/*
  this functions builds ont lli table from the lli_array according to the
  given size of data
*/
static void sep_build_lli_table(struct sep_lli_entry_t *lli_array_ptr, struct sep_lli_entry_t *lli_table_ptr, unsigned long *num_processed_entries_ptr, unsigned long *num_table_entries_ptr, unsigned long table_data_size);

/*
  this function goes over the list of the print created tables and prints
  all the data
*/
static void sep_debug_print_lli_tables(struct sep_lli_entry_t *lli_table_ptr, unsigned long num_table_entries, unsigned long table_data_size);



/*
  This function raises interrupt to SEPm that signals that is has a new
  command from HOST
*/
static void sep_send_command_handler(void);


/*
  This function raises interrupt to SEP that signals that is has a
  new reply from HOST
*/
static void sep_send_reply_command_handler(void);

/*
  This function handles the allocate data pool memory request
  This function returns calculates the physical address of the allocated memory
  and the offset of this area from the mapped address. Therefore, the FVOs in
  user space can calculate the exact virtual address of this allocated memory
*/
static int sep_allocate_data_pool_memory_handler(unsigned long arg);


/*
  This function  handles write into allocated data pool command
*/
static int sep_write_into_data_pool_handler(unsigned long arg);

/*
  this function handles the read from data pool command
*/
static int sep_read_from_data_pool_handler(unsigned long arg);

/*
  this function handles tha request for creation of the DMA table
  for the synchronic symmetric operations (AES,DES)
*/
static int sep_create_sync_dma_tables_handler(unsigned long arg);

/*
  this function handles the request to create the DMA tables for flow
*/
static int sep_create_flow_dma_tables_handler(unsigned long arg);

/*
  This API handles the end transaction request
*/
static int sep_end_transaction_handler(unsigned long arg);


/*
  this function handles add tables to flow
*/
static int sep_add_flow_tables_handler(unsigned long arg);

/*
  this function add the flow add message to the specific flow
*/
static int sep_add_flow_tables_message_handler(unsigned long arg);

/*
  this function handles the request for SEP start
*/
static int sep_start_handler(void);

/*
  this function handles the request for SEP initialization
*/
static int sep_init_handler(unsigned long arg);

/*
  this function handles the request cache and resident reallocation
*/
static int sep_realloc_cache_resident_handler(unsigned long arg);


/*
  This api handles the setting of API mode to blocking or non-blocking
*/
static int sep_set_api_mode_handler(unsigned long arg);

/* handler for flow done interrupt */
static void sep_flow_done_handler(struct work_struct *work);

/*
  This function locks all the physical pages of the kernel virtual buffer
  and construct a basic lli  array, where each entry holds the physical
  page address and the size that application data holds in this physical pages
*/
static int sep_lock_kernel_pages(unsigned long kernel_virt_addr, unsigned long data_size, unsigned long *num_pages_ptr, struct sep_lli_entry_t **lli_array_ptr, struct page ***page_array_ptr);

/*
  This function creates one DMA table for flow and returns its data,
  and pointer to its info entry
*/
static int sep_prepare_one_flow_dma_table(unsigned long virt_buff_addr, unsigned long virt_buff_size, struct sep_lli_entry_t *table_data, struct sep_lli_entry_t **info_entry_ptr, struct sep_flow_context_t *flow_data_ptr, bool isKernelVirtualAddress);

/*
  This function creates a list of tables for flow and returns the data for the
  first and last tables of the list
*/
static int sep_prepare_flow_dma_tables(unsigned long num_virtual_buffers,
				       unsigned long first_buff_addr, struct sep_flow_context_t *flow_data_ptr, struct sep_lli_entry_t *first_table_data_ptr, struct sep_lli_entry_t *last_table_data_ptr, bool isKernelVirtualAddress);

/*
  this function find a space for the new flow dma table
*/
static int sep_find_free_flow_dma_table_space(unsigned long **table_address_ptr);

/*
  this function goes over all the flow tables connected to the given table and
  deallocate them
*/
static void sep_deallocated_flow_tables(struct sep_lli_entry_t *first_table_ptr);

/*
  This function handler the set flow id command
*/
static int sep_set_flow_id_handler(unsigned long arg);

/*
  This function returns pointer to the  flow data structure
  that conatins the given id
*/
static int sep_find_flow_context(unsigned long flow_id, struct sep_flow_context_t **flow_data_ptr);


/*
  this function returns the physical and virtual addresses of the static pool
*/
static int sep_get_static_pool_addr_handler(unsigned long arg);

/*
  this address gets the offset of the physical address from the start of
  the mapped area
*/
static int sep_get_physical_mapped_offset_handler(unsigned long arg);


/*
  this function handles the request for get time
*/
static int sep_get_time_handler(unsigned long arg);

/*
  calculates time and sets it at the predefined address
*/
static int sep_set_time(unsigned long *address_ptr, unsigned long *time_in_sec_ptr);

/*
  PATCH for configuring the DMA to single burst instead of multi-burst
*/
static void sep_configure_dma_burst(void);

/*
	This function locks all the physical pages of the
	application virtual buffer and construct a basic lli
	array, where each entry holds the physical page address
	and the size that application data holds in this physical pages
*/
static int sep_lock_user_pages(unsigned long app_virt_addr, unsigned long data_size, unsigned long *num_pages_ptr, struct sep_lli_entry_t **lli_array_ptr, struct page ***page_array_ptr);

/*---------------------------------------------
	FUNCTIONS
-----------------------------------------------*/

/*
  this function returns the address of the message shared area
*/
void sep_map_shared_area(unsigned long *mappedAddr_ptr)
{
	*mappedAddr_ptr = sep_dev->shared_area_addr;
}

/*
  this function returns the address of the message shared area
*/
void sep_send_msg_rdy_cmd()
{
	sep_send_command_handler();
}

/* this functions frees all the resources that were allocated for the building
of the LLI DMA tables */
void sep_free_dma_resources()
{
	sep_free_dma_table_data_handler();
}

/* poll(suspend), until reply from sep */
void sep_driver_poll()
{
	unsigned long retVal = 0;

#ifdef SEP_DRIVER_POLLING_MODE

	while (sep_dev->host_to_sep_send_counter != (retVal & 0x7FFFFFFF))
		retVal = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR2_REG_ADDR);

	sep_dev->sep_to_host_reply_counter++;
#else
	/* poll, until reply from sep */
	wait_event(g_sep_event, (sep_dev->host_to_sep_send_counter == sep_dev->sep_to_host_reply_counter));

#endif
}

/*----------------------------------------------------------------------
  open function of the character driver - must only lock the mutex
	must also release the memory data pool allocations
------------------------------------------------------------------------*/
static int sep_open(struct inode *inode_ptr, struct file *file_ptr)
{
	int error;

	dbg("SEP Driver:--------> open start\n");

	error = 0;

	/* check the blocking mode */
	if (sep_dev->block_mode_flag)
		/* lock mutex */
		mutex_lock(&sep_mutex);
	else
		error = mutex_trylock(&sep_mutex);

	/* check the error */
	if (error) {
		edbg("SEP Driver: down_interruptible failed\n");

		goto end_function;
	}

	/* release data pool allocations */
	sep_dev->data_pool_bytes_allocated = 0;

end_function:
	dbg("SEP Driver:<-------- open end\n");
	return error;
}




/*------------------------------------------------------------
	release function
-------------------------------------------------------------*/
static int sep_release(struct inode *inode_ptr, struct file *file_ptr)
{
	dbg("----------->SEP Driver: sep_release start\n");

#if 0				/*!SEP_DRIVER_POLLING_MODE */
	/* close IMR */
	sep_write_reg(sep_dev, HW_HOST_IMR_REG_ADDR, 0x7FFF);

	/* release IRQ line */
	free_irq(SEP_DIRVER_IRQ_NUM, &sep_dev->reg_base_address);

#endif

	/* unlock the sep mutex */
	mutex_unlock(&sep_mutex);

	dbg("SEP Driver:<-------- sep_release end\n");

	return 0;
}




/*---------------------------------------------------------------
  map function - this functions maps the message shared area
-----------------------------------------------------------------*/
static int sep_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long phys_addr;

	dbg("-------->SEP Driver: mmap start\n");

	/* check that the size of the mapped range is as the size of the message
	   shared area */
	if ((vma->vm_end - vma->vm_start) > SEP_DRIVER_MMMAP_AREA_SIZE) {
		edbg("SEP Driver mmap requested size is more than allowed\n");
		printk(KERN_WARNING "SEP Driver mmap requested size is more \
			than allowed\n");
		printk(KERN_WARNING "SEP Driver vma->vm_end is %08lx\n", vma->vm_end);
		printk(KERN_WARNING "SEP Driver vma->vm_end is %08lx\n", vma->vm_start);
		return -EAGAIN;
	}

	edbg("SEP Driver:g_message_shared_area_addr is %08lx\n", sep_dev->message_shared_area_addr);

	/* get physical address */
	phys_addr = sep_dev->phys_shared_area_addr;

	edbg("SEP Driver: phys_addr is %08lx\n", phys_addr);

	if (remap_pfn_range(vma, vma->vm_start, phys_addr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		edbg("SEP Driver remap_page_range failed\n");
		printk(KERN_WARNING "SEP Driver remap_page_range failed\n");
		return -EAGAIN;
	}

	dbg("SEP Driver:<-------- mmap end\n");

	return 0;
}


/*-----------------------------------------------
  poll function
*----------------------------------------------*/
static unsigned int sep_poll(struct file *filp, poll_table * wait)
{
	unsigned long count;
	unsigned int mask = 0;
	unsigned long retVal = 0;	/* flow id */

	dbg("---------->SEP Driver poll: start\n");


#if SEP_DRIVER_POLLING_MODE

	while (sep_dev->host_to_sep_send_counter != (retVal & 0x7FFFFFFF)) {
		retVal = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR2_REG_ADDR);

		for (count = 0; count < 10 * 4; count += 4)
			edbg("Poll Debug Word %lu of the message is %lu\n", count, *((unsigned long *) (sep_dev->shared_area_addr + SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES + count)));
	}

	sep_dev->sep_to_host_reply_counter++;
#else
	/* add the event to the polling wait table */
	poll_wait(filp, &g_sep_event, wait);

#endif

	edbg("sep_dev->host_to_sep_send_counter is %lu\n", sep_dev->host_to_sep_send_counter);
	edbg("sep_dev->sep_to_host_reply_counter is %lu\n", sep_dev->sep_to_host_reply_counter);

	/* check if the data is ready */
	if (sep_dev->host_to_sep_send_counter == sep_dev->sep_to_host_reply_counter) {
		for (count = 0; count < 12 * 4; count += 4)
			edbg("Sep Mesg Word %lu of the message is %lu\n", count, *((unsigned long *) (sep_dev->shared_area_addr + count)));

		for (count = 0; count < 10 * 4; count += 4)
			edbg("Debug Data Word %lu of the message is %lu\n", count, *((unsigned long *) (sep_dev->shared_area_addr + 0x1800 + count)));

		retVal = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
		edbg("retVal is %lu\n", retVal);
		/* check if the this is sep reply or request */
		if (retVal >> 31) {
			edbg("SEP Driver: sep request in\n");
			/* request */
			mask |= POLLOUT | POLLWRNORM;
		} else {
			edbg("SEP Driver: sep reply in\n");
			mask |= POLLIN | POLLRDNORM;
		}
	}
	dbg("SEP Driver:<-------- poll exit\n");
	return mask;
}


static int sep_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int error = 0;

	dbg("------------>SEP Driver: ioctl start\n");

	edbg("SEP Driver: cmd is %x\n", cmd);

	/* check that the command is for sep device */
	if (_IOC_TYPE(cmd) != SEP_IOC_MAGIC_NUMBER)
		error = -ENOTTY;

	switch (cmd) {
	case SEP_IOCSENDSEPCOMMAND:
		/* send command to SEP */
		sep_send_command_handler();
		edbg("SEP Driver: after sep_send_command_handler\n");
		break;
	case SEP_IOCSENDSEPRPLYCOMMAND:
		/* send reply command to SEP */
		sep_send_reply_command_handler();
		break;
	case SEP_IOCALLOCDATAPOLL:
		/* allocate data pool */
		error = sep_allocate_data_pool_memory_handler(arg);
		break;
	case SEP_IOCWRITEDATAPOLL:
		/* write data into memory pool */
		error = sep_write_into_data_pool_handler(arg);
		break;
	case SEP_IOCREADDATAPOLL:
		/* read data from data pool into application memory */
		error = sep_read_from_data_pool_handler(arg);
		break;
	case SEP_IOCCREATESYMDMATABLE:
		/* create dma table for synhronic operation */
		error = sep_create_sync_dma_tables_handler(arg);
		break;
	case SEP_IOCCREATEFLOWDMATABLE:
		/* create flow dma tables */
		error = sep_create_flow_dma_tables_handler(arg);
		break;
	case SEP_IOCFREEDMATABLEDATA:
		/* free the pages */
		error = sep_free_dma_table_data_handler();
		break;
	case SEP_IOCSETFLOWID:
		/* set flow id */
		error = sep_set_flow_id_handler(arg);
		break;
	case SEP_IOCADDFLOWTABLE:
		/* add tables to the dynamic flow */
		error = sep_add_flow_tables_handler(arg);
		break;
	case SEP_IOCADDFLOWMESSAGE:
		/* add message of add tables to flow */
		error = sep_add_flow_tables_message_handler(arg);
		break;
	case SEP_IOCSEPSTART:
		/* start command to sep */
		error = sep_start_handler();
		break;
	case SEP_IOCSEPINIT:
		/* init command to sep */
		error = sep_init_handler(arg);
		break;
	case SEP_IOCSETAPIMODE:
		/* set non- blocking mode */
		error = sep_set_api_mode_handler(arg);
		break;
	case SEP_IOCGETSTATICPOOLADDR:
		/* get the physical and virtual addresses of the static pool */
		error = sep_get_static_pool_addr_handler(arg);
		break;
	case SEP_IOCENDTRANSACTION:
		error = sep_end_transaction_handler(arg);
		break;
	case SEP_IOCREALLOCCACHERES:
		error = sep_realloc_cache_resident_handler(arg);
		break;
	case SEP_IOCGETMAPPEDADDROFFSET:
		error = sep_get_physical_mapped_offset_handler(arg);
		break;
	case SEP_IOCGETIME:
		error = sep_get_time_handler(arg);
		break;
	default:
		error = -ENOTTY;
		break;
	}
	dbg("SEP Driver:<-------- ioctl end\n");
	return error;
}



/*
  interrupt handler function
*/
irqreturn_t sep_inthandler(int irq, void *dev_id)
{
	irqreturn_t int_error;
	unsigned long error;
	unsigned long reg_val;
	unsigned long flow_id;
	struct sep_flow_context_t *flow_context_ptr;

	int_error = IRQ_HANDLED;

	/* read the IRR register to check if this is SEP interrupt */
	reg_val = sep_read_reg(sep_dev, HW_HOST_IRR_REG_ADDR);
	edbg("SEP Interrupt - reg is %08lx\n", reg_val);

	/* check if this is the flow interrupt */
	if (0 /*reg_val & (0x1 << 11) */ ) {
		/* read GPRO to find out the which flow is done */
		flow_id = sep_read_reg(sep_dev, HW_HOST_IRR_REG_ADDR);

		/* find the contex of the flow */
		error = sep_find_flow_context(flow_id >> 28, &flow_context_ptr);
		if (error)
			goto end_function_with_error;

		INIT_WORK(&flow_context_ptr->flow_wq, sep_flow_done_handler);

		/* queue the work */
		queue_work(sep_dev->flow_wq_ptr, &flow_context_ptr->flow_wq);

	} else {
		/* check if this is reply interrupt from SEP */
		if (reg_val & (0x1 << 13)) {
			/* update the counter of reply messages */
			sep_dev->sep_to_host_reply_counter++;

			/* wake up the waiting process */
			wake_up(&g_sep_event);
		} else {
			int_error = IRQ_NONE;
			goto end_function;
		}
	}
end_function_with_error:
	/* clear the interrupt */
	sep_write_reg(sep_dev, HW_HOST_ICR_REG_ADDR, reg_val);
end_function:
	return int_error;
}


/*
  This function prepares only input DMA table for synhronic symmetric
  operations (HASH)
*/
int sep_prepare_input_dma_table(unsigned long app_virt_addr, unsigned long data_size, unsigned long block_size, unsigned long *lli_table_ptr, unsigned long *num_entries_ptr, unsigned long *table_data_size_ptr, bool isKernelVirtualAddress)
{
	/* pointer to the info entry of the table - the last entry */
	struct sep_lli_entry_t *info_entry_ptr;
	/* array of pointers ot page */
	struct sep_lli_entry_t *lli_array_ptr;
	/* points to the first entry to be processed in the lli_in_array */
	unsigned long current_entry;
	/* num entries in the virtual buffer */
	unsigned long sep_lli_entries;
	/* lli table pointer */
	struct sep_lli_entry_t *in_lli_table_ptr;
	/* the total data in one table */
	unsigned long table_data_size;
	/* number of entries in lli table */
	unsigned long num_entries_in_table;
	/* next table address */
	unsigned long lli_table_alloc_addr;
	unsigned long result;

	dbg("SEP Driver:--------> sep_prepare_input_dma_table start\n");

	edbg("SEP Driver:data_size is %lu\n", data_size);
	edbg("SEP Driver:block_size is %lu\n", block_size);

	/* initialize the pages pointers */
	sep_dev->in_page_array = 0;
	sep_dev->in_num_pages = 0;

	if (data_size == 0) {
		/* special case  - created 2 entries table with zero data */
		in_lli_table_ptr = (struct sep_lli_entry_t *) (sep_dev->shared_area_addr + SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_OFFSET_IN_BYTES);
		in_lli_table_ptr->physical_address = sep_dev->shared_area_addr + SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_OFFSET_IN_BYTES;
		in_lli_table_ptr->block_size = 0;

		in_lli_table_ptr++;
		in_lli_table_ptr->physical_address = 0xFFFFFFFF;
		in_lli_table_ptr->block_size = 0;

		*lli_table_ptr = sep_dev->phys_shared_area_addr + SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_OFFSET_IN_BYTES;
		*num_entries_ptr = 2;
		*table_data_size_ptr = 0;

		goto end_function;
	}

	/* check if the pages are in Kernel Virtual Address layout */
	if (isKernelVirtualAddress == true)
		/* lock the pages of the kernel buffer and translate them to pages */
		result = sep_lock_kernel_pages(app_virt_addr, data_size, &sep_dev->in_num_pages, &lli_array_ptr, &sep_dev->in_page_array);
	else
		/* lock the pages of the user buffer and translate them to pages */
		result = sep_lock_user_pages(app_virt_addr, data_size, &sep_dev->in_num_pages, &lli_array_ptr, &sep_dev->in_page_array);

	if (result)
		return result;

	edbg("SEP Driver:output sep_dev->in_num_pages is %lu\n", sep_dev->in_num_pages);

	current_entry = 0;
	info_entry_ptr = 0;
	sep_lli_entries = sep_dev->in_num_pages;

	/* initiate to point after the message area */
	lli_table_alloc_addr = sep_dev->shared_area_addr + SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_OFFSET_IN_BYTES;

	/* loop till all the entries in in array are not processed */
	while (current_entry < sep_lli_entries) {
		/* set the new input and output tables */
		in_lli_table_ptr = (struct sep_lli_entry_t *) lli_table_alloc_addr;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry_t) * SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		/* calculate the maximum size of data for input table */
		table_data_size = sep_calculate_lli_table_max_size(&lli_array_ptr[current_entry], (sep_lli_entries - current_entry));

		/* now calculate the table size so that it will be module block size */
		table_data_size = (table_data_size / block_size) * block_size;

		edbg("SEP Driver:output table_data_size is %lu\n", table_data_size);

		/* construct input lli table */
		sep_build_lli_table(&lli_array_ptr[current_entry], in_lli_table_ptr, &current_entry, &num_entries_in_table, table_data_size);

		if (info_entry_ptr == 0) {
			/* set the output parameters to physical addresses */
			*lli_table_ptr = sep_shared_area_virt_to_phys((unsigned long) in_lli_table_ptr);
			*num_entries_ptr = num_entries_in_table;
			*table_data_size_ptr = table_data_size;

			edbg("SEP Driver:output lli_table_in_ptr is %08lx\n", *lli_table_ptr);
		} else {
			/* update the info entry of the previous in table */
			info_entry_ptr->physical_address = sep_shared_area_virt_to_phys((unsigned long) in_lli_table_ptr);
			info_entry_ptr->block_size = ((num_entries_in_table) << 24) | (table_data_size);
		}

		/* save the pointer to the info entry of the current tables */
		info_entry_ptr = in_lli_table_ptr + num_entries_in_table - 1;
	}

	/* print input tables */
	sep_debug_print_lli_tables((struct sep_lli_entry_t *)
				   sep_shared_area_phys_to_virt(*lli_table_ptr), *num_entries_ptr, *table_data_size_ptr);

	/* the array of the pages */
	kfree(lli_array_ptr);
end_function:
	dbg("SEP Driver:<-------- sep_prepare_input_dma_table end\n");
	return 0;

}

/*
  This function builds input and output DMA tables for synhronic
  symmetric operations (AES, DES). It also checks that each table
  is of the modular block size
*/
int sep_prepare_input_output_dma_table(unsigned long app_virt_in_addr,
				       unsigned long app_virt_out_addr,
				       unsigned long data_size,
				       unsigned long block_size,
				       unsigned long *lli_table_in_ptr, unsigned long *lli_table_out_ptr, unsigned long *in_num_entries_ptr, unsigned long *out_num_entries_ptr, unsigned long *table_data_size_ptr, bool isKernelVirtualAddress)
{
	/* array of pointers of page */
	struct sep_lli_entry_t *lli_in_array;
	/* array of pointers of page */
	struct sep_lli_entry_t *lli_out_array;
	int result = 0;

	dbg("SEP Driver:--------> sep_prepare_input_output_dma_table start\n");

	/* initialize the pages pointers */
	sep_dev->in_page_array = 0;
	sep_dev->out_page_array = 0;

	/* check if the pages are in Kernel Virtual Address layout */
	if (isKernelVirtualAddress == true) {
		/* lock the pages of the kernel buffer and translate them to pages */
		result = sep_lock_kernel_pages(app_virt_in_addr, data_size, &sep_dev->in_num_pages, &lli_in_array, &sep_dev->in_page_array);
		if (result) {
			edbg("SEP Driver: sep_lock_kernel_pages for input virtual buffer failed\n");
			goto end_function;
		}
	} else {
		/* lock the pages of the user buffer and translate them to pages */
		result = sep_lock_user_pages(app_virt_in_addr, data_size, &sep_dev->in_num_pages, &lli_in_array, &sep_dev->in_page_array);
		if (result) {
			edbg("SEP Driver: sep_lock_user_pages for input virtual buffer failed\n");
			goto end_function;
		}
	}

	if (isKernelVirtualAddress == true) {
		result = sep_lock_kernel_pages(app_virt_out_addr, data_size, &sep_dev->out_num_pages, &lli_out_array, &sep_dev->out_page_array);
		if (result) {
			edbg("SEP Driver: sep_lock_kernel_pages for output virtual buffer failed\n");
			goto end_function_with_error1;
		}
	} else {
		result = sep_lock_user_pages(app_virt_out_addr, data_size, &sep_dev->out_num_pages, &lli_out_array, &sep_dev->out_page_array);
		if (result) {
			edbg("SEP Driver: sep_lock_user_pages for output virtual buffer failed\n");
			goto end_function_with_error1;
		}
	}
	edbg("sep_dev->in_num_pages is %lu\n", sep_dev->in_num_pages);
	edbg("sep_dev->out_num_pages is %lu\n", sep_dev->out_num_pages);
	edbg("SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP is %x\n", SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP);


	/* call the fucntion that creates table from the lli arrays */
	result = sep_construct_dma_tables_from_lli(lli_in_array, sep_dev->in_num_pages, lli_out_array, sep_dev->out_num_pages, block_size, lli_table_in_ptr, lli_table_out_ptr, in_num_entries_ptr, out_num_entries_ptr, table_data_size_ptr);
	if (result) {
		edbg("SEP Driver: sep_construct_dma_tables_from_lli failed\n");
		goto end_function_with_error2;
	}

	/* fall through - free the lli entry arrays */
	dbg("in_num_entries_ptr is %08lx\n", *in_num_entries_ptr);
	dbg("out_num_entries_ptr is %08lx\n", *out_num_entries_ptr);
	dbg("table_data_size_ptr is %08lx\n", *table_data_size_ptr);
end_function_with_error2:
	kfree(lli_out_array);
end_function_with_error1:
	kfree(lli_in_array);
end_function:
	dbg("SEP Driver:<-------- sep_prepare_input_output_dma_table end result = %d\n", (int) result);
	return result;

}


/*
 This function creates the input and output dma tables for
 symmetric operations (AES/DES) according to the block size from LLI arays
*/
int sep_construct_dma_tables_from_lli(struct sep_lli_entry_t *lli_in_array,
				      unsigned long sep_in_lli_entries,
				      struct sep_lli_entry_t *lli_out_array,
				      unsigned long sep_out_lli_entries,
				      unsigned long block_size, unsigned long *lli_table_in_ptr, unsigned long *lli_table_out_ptr, unsigned long *in_num_entries_ptr, unsigned long *out_num_entries_ptr, unsigned long *table_data_size_ptr)
{
	/* points to the area where next lli table can be allocated */
	unsigned long lli_table_alloc_addr;
	/* input lli table */
	struct sep_lli_entry_t *in_lli_table_ptr;
	/* output lli table */
	struct sep_lli_entry_t *out_lli_table_ptr;
	/* pointer to the info entry of the table - the last entry */
	struct sep_lli_entry_t *info_in_entry_ptr;
	/* pointer to the info entry of the table - the last entry */
	struct sep_lli_entry_t *info_out_entry_ptr;
	/* points to the first entry to be processed in the lli_in_array */
	unsigned long current_in_entry;
	/* points to the first entry to be processed in the lli_out_array */
	unsigned long current_out_entry;
	/* max size of the input table */
	unsigned long in_table_data_size;
	/* max size of the output table */
	unsigned long out_table_data_size;
	/* flag te signifies if this is the first tables build from the arrays */
	unsigned long first_table_flag;
	/* the data size that should be in table */
	unsigned long table_data_size;
	/* number of etnries in the input table */
	unsigned long num_entries_in_table;
	/* number of etnries in the output table */
	unsigned long num_entries_out_table;

	dbg("SEP Driver:--------> sep_construct_dma_tables_from_lli start\n");

	/* initiate to pint after the message area */
	lli_table_alloc_addr = sep_dev->shared_area_addr + SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_OFFSET_IN_BYTES;

	current_in_entry = 0;
	current_out_entry = 0;
	first_table_flag = 1;
	info_in_entry_ptr = 0;
	info_out_entry_ptr = 0;

	/* loop till all the entries in in array are not processed */
	while (current_in_entry < sep_in_lli_entries) {
		/* set the new input and output tables */
		in_lli_table_ptr = (struct sep_lli_entry_t *) lli_table_alloc_addr;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry_t) * SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		/* set the first output tables */
		out_lli_table_ptr = (struct sep_lli_entry_t *) lli_table_alloc_addr;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry_t) * SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		/* calculate the maximum size of data for input table */
		in_table_data_size = sep_calculate_lli_table_max_size(&lli_in_array[current_in_entry], (sep_in_lli_entries - current_in_entry));

		/* calculate the maximum size of data for output table */
		out_table_data_size = sep_calculate_lli_table_max_size(&lli_out_array[current_out_entry], (sep_out_lli_entries - current_out_entry));

		edbg("SEP Driver:in_table_data_size is %lu\n", in_table_data_size);
		edbg("SEP Driver:out_table_data_size is %lu\n", out_table_data_size);

		/* check where the data is smallest */
		table_data_size = in_table_data_size;
		if (table_data_size > out_table_data_size)
			table_data_size = out_table_data_size;

		/* now calculate the table size so that it will be module block size */
		table_data_size = (table_data_size / block_size) * block_size;

		dbg("SEP Driver:table_data_size is %lu\n", table_data_size);

		/* construct input lli table */
		sep_build_lli_table(&lli_in_array[current_in_entry], in_lli_table_ptr, &current_in_entry, &num_entries_in_table, table_data_size);

		/* construct output lli table */
		sep_build_lli_table(&lli_out_array[current_out_entry], out_lli_table_ptr, &current_out_entry, &num_entries_out_table, table_data_size);

		/* if info entry is null - this is the first table built */
		if (info_in_entry_ptr == 0) {
			/* set the output parameters to physical addresses */
			*lli_table_in_ptr = sep_shared_area_virt_to_phys((unsigned long) in_lli_table_ptr);
			*in_num_entries_ptr = num_entries_in_table;
			*lli_table_out_ptr = sep_shared_area_virt_to_phys((unsigned long) out_lli_table_ptr);
			*out_num_entries_ptr = num_entries_out_table;
			*table_data_size_ptr = table_data_size;

			edbg("SEP Driver:output lli_table_in_ptr is %08lx\n", *lli_table_in_ptr);
			edbg("SEP Driver:output lli_table_out_ptr is %08lx\n", *lli_table_out_ptr);
		} else {
			/* update the info entry of the previous in table */
			info_in_entry_ptr->physical_address = sep_shared_area_virt_to_phys((unsigned long) in_lli_table_ptr);
			info_in_entry_ptr->block_size = ((num_entries_in_table) << 24) | (table_data_size);

			/* update the info entry of the previous in table */
			info_out_entry_ptr->physical_address = sep_shared_area_virt_to_phys((unsigned long) out_lli_table_ptr);
			info_out_entry_ptr->block_size = ((num_entries_out_table) << 24) | (table_data_size);
		}

		/* save the pointer to the info entry of the current tables */
		info_in_entry_ptr = in_lli_table_ptr + num_entries_in_table - 1;
		info_out_entry_ptr = out_lli_table_ptr + num_entries_out_table - 1;

		edbg("SEP Driver:output num_entries_out_table is %lu\n", (unsigned long) num_entries_out_table);
		edbg("SEP Driver:output info_in_entry_ptr is %lu\n", (unsigned long) info_in_entry_ptr);
		edbg("SEP Driver:output info_out_entry_ptr is %lu\n", (unsigned long) info_out_entry_ptr);
	}

	/* print input tables */
	sep_debug_print_lli_tables((struct sep_lli_entry_t *)
				   sep_shared_area_phys_to_virt(*lli_table_in_ptr), *in_num_entries_ptr, *table_data_size_ptr);
	/* print output tables */
	sep_debug_print_lli_tables((struct sep_lli_entry_t *)
				   sep_shared_area_phys_to_virt(*lli_table_out_ptr), *out_num_entries_ptr, *table_data_size_ptr);
	dbg("SEP Driver:<-------- sep_construct_dma_tables_from_lli end\n");
	return 0;
}

/*
  this function calculates the size of data that can be inserted into the lli
  table from this array the condition is that either the table is full
  (all etnries are entered), or there are no more entries in the lli array
*/
unsigned long sep_calculate_lli_table_max_size(struct sep_lli_entry_t *lli_in_array_ptr, unsigned long num_array_entries)
{
	unsigned long table_data_size = 0;
	unsigned long counter;

	/* calculate the data in the out lli table if till we fill the whole
	   table or till the data has ended */
	for (counter = 0; (counter < (SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP - 1)) && (counter < num_array_entries); counter++)
		table_data_size += lli_in_array_ptr[counter].block_size;
	return table_data_size;
}

/*
  this functions builds ont lli table from the lli_array according to
  the given size of data
*/
static void sep_build_lli_table(struct sep_lli_entry_t *lli_array_ptr, struct sep_lli_entry_t *lli_table_ptr, unsigned long *num_processed_entries_ptr, unsigned long *num_table_entries_ptr, unsigned long table_data_size)
{
	unsigned long curr_table_data_size;
	/* counter of lli array entry */
	unsigned long array_counter;

	dbg("SEP Driver:--------> sep_build_lli_table start\n");

	/* init currrent table data size and lli array entry counter */
	curr_table_data_size = 0;
	array_counter = 0;
	*num_table_entries_ptr = 1;

	edbg("SEP Driver:table_data_size is %lu\n", table_data_size);

	/* fill the table till table size reaches the needed amount */
	while (curr_table_data_size < table_data_size) {
		/* update the number of entries in table */
		(*num_table_entries_ptr)++;

		lli_table_ptr->physical_address = lli_array_ptr[array_counter].physical_address;
		lli_table_ptr->block_size = lli_array_ptr[array_counter].block_size;
		curr_table_data_size += lli_table_ptr->block_size;

		edbg("SEP Driver:lli_table_ptr is %08lx\n", (unsigned long) lli_table_ptr);
		edbg("SEP Driver:lli_table_ptr->physical_address is %08lx\n", lli_table_ptr->physical_address);
		edbg("SEP Driver:lli_table_ptr->block_size is %lu\n", lli_table_ptr->block_size);

		/* check for overflow of the table data */
		if (curr_table_data_size > table_data_size) {
			edbg("SEP Driver:curr_table_data_size > table_data_size\n");

			/* update the size of block in the table */
			lli_table_ptr->block_size -= (curr_table_data_size - table_data_size);

			/* update the physical address in the lli array */
			lli_array_ptr[array_counter].physical_address += lli_table_ptr->block_size;

			/* update the block size left in the lli array */
			lli_array_ptr[array_counter].block_size = (curr_table_data_size - table_data_size);
		} else
			/* advance to the next entry in the lli_array */
			array_counter++;

		edbg("SEP Driver:lli_table_ptr->physical_address is %08lx\n", lli_table_ptr->physical_address);
		edbg("SEP Driver:lli_table_ptr->block_size is %lu\n", lli_table_ptr->block_size);

		/* move to the next entry in table */
		lli_table_ptr++;
	}

	/* set the info entry to default */
	lli_table_ptr->physical_address = 0xffffffff;
	lli_table_ptr->block_size = 0;

	edbg("SEP Driver:lli_table_ptr is %08lx\n", (unsigned long) lli_table_ptr);
	edbg("SEP Driver:lli_table_ptr->physical_address is %08lx\n", lli_table_ptr->physical_address);
	edbg("SEP Driver:lli_table_ptr->block_size is %lu\n", lli_table_ptr->block_size);

	/* set the output parameter */
	*num_processed_entries_ptr += array_counter;

	edbg("SEP Driver:*num_processed_entries_ptr is %lu\n", *num_processed_entries_ptr);
	dbg("SEP Driver:<-------- sep_build_lli_table end\n");
	return;
}

/*
  this function goes over the list of the print created tables and
  prints all the data
*/
static void sep_debug_print_lli_tables(struct sep_lli_entry_t *lli_table_ptr, unsigned long num_table_entries, unsigned long table_data_size)
{
	unsigned long table_count;
	unsigned long entries_count;

	dbg("SEP Driver:--------> sep_debug_print_lli_tables start\n");

	table_count = 1;
	while ((unsigned long) lli_table_ptr != 0xffffffff) {
		edbg("SEP Driver: lli table %08lx, table_data_size is %lu\n", table_count, table_data_size);
		edbg("SEP Driver: num_table_entries is %lu\n", num_table_entries);

		/* print entries of the table (without info entry) */
		for (entries_count = 0; entries_count < num_table_entries; entries_count++, lli_table_ptr++) {
			edbg("SEP Driver:lli_table_ptr address is %08lx\n", (unsigned long) lli_table_ptr);
			edbg("SEP Driver:phys address is %08lx block size is %lu\n", lli_table_ptr->physical_address, lli_table_ptr->block_size);
		}

		/* point to the info entry */
		lli_table_ptr--;

		edbg("SEP Driver:phys lli_table_ptr->block_size is %lu\n", lli_table_ptr->block_size);
		edbg("SEP Driver:phys lli_table_ptr->physical_address is %08lx\n", lli_table_ptr->physical_address);


		table_data_size = lli_table_ptr->block_size & 0xffffff;
		num_table_entries = (lli_table_ptr->block_size >> 24) & 0xff;
		lli_table_ptr = (struct sep_lli_entry_t *)
		    (lli_table_ptr->physical_address);

		edbg("SEP Driver:phys table_data_size is %lu num_table_entries is %lu lli_table_ptr is%lu\n", table_data_size, num_table_entries, (unsigned long) lli_table_ptr);

		if ((unsigned long) lli_table_ptr != 0xffffffff)
			lli_table_ptr = (struct sep_lli_entry_t *) sep_shared_area_phys_to_virt((unsigned long) lli_table_ptr);

		table_count++;
	}
	dbg("SEP Driver:<-------- sep_debug_print_lli_tables end\n");
}


/*
  This function locks all the physical pages of the application virtual buffer
  and construct a basic lli  array, where each entry holds the physical page
  address and the size that application data holds in this physical pages
*/
int sep_lock_user_pages(unsigned long app_virt_addr, unsigned long data_size, unsigned long *num_pages_ptr, struct sep_lli_entry_t **lli_array_ptr, struct page ***page_array_ptr)
{
	int error = 0;
	/* the the page of the end address of the user space buffer */
	unsigned long end_page;
	/* the page of the start address of the user space buffer */
	unsigned long start_page;
	/* the range in pages */
	unsigned long num_pages;
	struct page **page_array;
	struct sep_lli_entry_t *lli_array;
	unsigned long count;
	int result;

	dbg("SEP Driver:--------> sep_lock_user_pages start\n");

	/* set start and end pages  and num pages */
	end_page = (app_virt_addr + data_size - 1) >> PAGE_SHIFT;
	start_page = app_virt_addr >> PAGE_SHIFT;
	num_pages = end_page - start_page + 1;

	edbg("SEP Driver: app_virt_addr is %08lx\n", app_virt_addr);
	edbg("SEP Driver: data_size is %lu\n", data_size);
	edbg("SEP Driver: start_page is %lu\n", start_page);
	edbg("SEP Driver: end_page is %lu\n", end_page);
	edbg("SEP Driver: num_pages is %lu\n", num_pages);

	/* allocate array of pages structure pointers */
	page_array = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!page_array) {
		edbg("SEP Driver: kmalloc for page_array failed\n");

		error = -ENOMEM;
		goto end_function;
	}

	lli_array = kmalloc(sizeof(struct sep_lli_entry_t) * num_pages, GFP_ATOMIC);
	if (!lli_array) {
		edbg("SEP Driver: kmalloc for lli_array failed\n");

		error = -ENOMEM;
		goto end_function_with_error1;
	}

	/* convert the application virtual address into a set of physical */
	down_read(&current->mm->mmap_sem);
	result = get_user_pages(current, current->mm, app_virt_addr, num_pages, 1, 0, page_array, 0);
	up_read(&current->mm->mmap_sem);

	/* check the number of pages locked - if not all then exit with error */
	if (result != num_pages) {
		dbg("SEP Driver: not all pages locked by get_user_pages\n");

		error = -ENOMEM;
		goto end_function_with_error2;
	}

	/* flush the cache */
	for (count = 0; count < num_pages; count++)
		flush_dcache_page(page_array[count]);

	/* set the start address of the first page - app data may start not at
	   the beginning of the page */
	lli_array[0].physical_address = ((unsigned long) page_to_phys(page_array[0])) + (app_virt_addr & (~PAGE_MASK));

	/* check that not all the data is in the first page only */
	if ((PAGE_SIZE - (app_virt_addr & (~PAGE_MASK))) >= data_size)
		lli_array[0].block_size = data_size;
	else
		lli_array[0].block_size = PAGE_SIZE - (app_virt_addr & (~PAGE_MASK));

	/* debug print */
	dbg("lli_array[0].physical_address is %08lx, lli_array[0].block_size is %lu\n", lli_array[0].physical_address, lli_array[0].block_size);

	/* go from the second page to the prev before last */
	for (count = 1; count < (num_pages - 1); count++) {
		lli_array[count].physical_address = (unsigned long) page_to_phys(page_array[count]);
		lli_array[count].block_size = PAGE_SIZE;

		edbg("lli_array[%lu].physical_address is %08lx, lli_array[%lu].block_size is %lu\n", count, lli_array[count].physical_address, count, lli_array[count].block_size);
	}

	/* if more then 1 pages locked - then update for the last page size needed */
	if (num_pages > 1) {
		/* update the address of the last page */
		lli_array[count].physical_address = (unsigned long) page_to_phys(page_array[count]);

		/* set the size of the last page */
		lli_array[count].block_size = (app_virt_addr + data_size) & (~PAGE_MASK);

		if (lli_array[count].block_size == 0) {
			dbg("app_virt_addr is %08lx\n", app_virt_addr);
			dbg("data_size is %lu\n", data_size);
			while (1);
		}
		edbg("lli_array[%lu].physical_address is %08lx, \
		lli_array[%lu].block_size is %lu\n", count, lli_array[count].physical_address, count, lli_array[count].block_size);
	}

	/* set output params */
	*lli_array_ptr = lli_array;
	*num_pages_ptr = num_pages;
	*page_array_ptr = page_array;
	goto end_function;

end_function_with_error2:
	/* release the cache */
	for (count = 0; count < num_pages; count++)
		page_cache_release(page_array[count]);
	kfree(lli_array);
end_function_with_error1:
	kfree(page_array);
end_function:
	dbg("SEP Driver:<-------- sep_lock_user_pages end\n");
	return 0;
}

/*
  This function locks all the physical pages of the kernel virtual buffer
  and construct a basic lli  array, where each entry holds the physical
  page address and the size that application data holds in this physical pages
*/
int sep_lock_kernel_pages(unsigned long kernel_virt_addr, unsigned long data_size, unsigned long *num_pages_ptr, struct sep_lli_entry_t **lli_array_ptr, struct page ***page_array_ptr)
{
	int error = 0;
	/* the the page of the end address of the user space buffer */
	unsigned long end_page;
	/* the page of the start address of the user space buffer */
	unsigned long start_page;
	/* the range in pages */
	unsigned long num_pages;
	struct sep_lli_entry_t *lli_array;
	/* next kernel address to map */
	unsigned long next_kernel_address;
	unsigned long count;

	dbg("SEP Driver:--------> sep_lock_kernel_pages start\n");

	/* set start and end pages  and num pages */
	end_page = (kernel_virt_addr + data_size - 1) >> PAGE_SHIFT;
	start_page = kernel_virt_addr >> PAGE_SHIFT;
	num_pages = end_page - start_page + 1;

	edbg("SEP Driver: kernel_virt_addr is %08lx\n", kernel_virt_addr);
	edbg("SEP Driver: data_size is %lu\n", data_size);
	edbg("SEP Driver: start_page is %lx\n", start_page);
	edbg("SEP Driver: end_page is %lx\n", end_page);
	edbg("SEP Driver: num_pages is %lu\n", num_pages);

	lli_array = kmalloc(sizeof(struct sep_lli_entry_t) * num_pages, GFP_ATOMIC);
	if (!lli_array) {
		edbg("SEP Driver: kmalloc for lli_array failed\n");
		error = -ENOMEM;
		goto end_function;
	}

	/* set the start address of the first page - app data may start not at
	   the beginning of the page */
	lli_array[0].physical_address = (unsigned long) virt_to_phys((unsigned long *) kernel_virt_addr);

	/* check that not all the data is in the first page only */
	if ((PAGE_SIZE - (kernel_virt_addr & (~PAGE_MASK))) >= data_size)
		lli_array[0].block_size = data_size;
	else
		lli_array[0].block_size = PAGE_SIZE - (kernel_virt_addr & (~PAGE_MASK));

	/* debug print */
	dbg("lli_array[0].physical_address is %08lx, lli_array[0].block_size is %lu\n", lli_array[0].physical_address, lli_array[0].block_size);

	/* advance the address to the start of the next page */
	next_kernel_address = (kernel_virt_addr & PAGE_MASK) + PAGE_SIZE;

	/* go from the second page to the prev before last */
	for (count = 1; count < (num_pages - 1); count++) {
		lli_array[count].physical_address = (unsigned long) virt_to_phys((unsigned long *) next_kernel_address);
		lli_array[count].block_size = PAGE_SIZE;

		edbg("lli_array[%lu].physical_address is %08lx, lli_array[%lu].block_size is %lu\n", count, lli_array[count].physical_address, count, lli_array[count].block_size);
		next_kernel_address += PAGE_SIZE;
	}

	/* if more then 1 pages locked - then update for the last page size needed */
	if (num_pages > 1) {
		/* update the address of the last page */
		lli_array[count].physical_address = (unsigned long) virt_to_phys((unsigned long *) next_kernel_address);

		/* set the size of the last page */
		lli_array[count].block_size = (kernel_virt_addr + data_size) & (~PAGE_MASK);

		if (lli_array[count].block_size == 0) {
			dbg("app_virt_addr is %08lx\n", kernel_virt_addr);
			dbg("data_size is %lu\n", data_size);
			while (1);
		}

		edbg("lli_array[%lu].physical_address is %08lx, lli_array[%lu].block_size is %lu\n", count, lli_array[count].physical_address, count, lli_array[count].block_size);
	}
	/* set output params */
	*lli_array_ptr = lli_array;
	*num_pages_ptr = num_pages;
	*page_array_ptr = 0;
end_function:
	dbg("SEP Driver:<-------- sep_lock_kernel_pages end\n");
	return 0;
}

/*
  This function releases all the application virtual buffer physical pages,
	that were previously locked
*/
int sep_free_dma_pages(struct page **page_array_ptr, unsigned long num_pages, unsigned long dirtyFlag)
{
	unsigned long count;

	if (dirtyFlag) {
		for (count = 0; count < num_pages; count++) {
			/* the out array was written, therefore the data was changed */
			if (!PageReserved(page_array_ptr[count]))
				SetPageDirty(page_array_ptr[count]);
			page_cache_release(page_array_ptr[count]);
		}
	} else {
		/* free in pages - the data was only read, therefore no update was done
		   on those pages */
		for (count = 0; count < num_pages; count++)
			page_cache_release(page_array_ptr[count]);
	}

	if (page_array_ptr)
		/* free the array */
		kfree(page_array_ptr);

	return 0;
}

/*
  This function raises interrupt to SEP that signals that is has a new
	command from HOST
*/
static void sep_send_command_handler()
{
	unsigned long count;

	dbg("SEP Driver:--------> sep_send_command_handler start\n");
	sep_set_time(0, 0);

	/* flash cache */
	flush_cache_all();

	for (count = 0; count < 12 * 4; count += 4)
		edbg("Word %lu of the message is %lu\n", count, *((unsigned long *) (sep_dev->shared_area_addr + count)));

	/* update counter */
	sep_dev->host_to_sep_send_counter++;
	/* send interrupt to SEP */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR0_REG_ADDR, 0x2);
	dbg("SEP Driver:<-------- sep_send_command_handler end\n");
	return;
}

/*
  This function raises interrupt to SEPm that signals that is has a
  new command from HOST
*/
static void sep_send_reply_command_handler()
{
	unsigned long count;

	dbg("SEP Driver:--------> sep_send_reply_command_handler start\n");

	/* flash cache */
	flush_cache_all();
	for (count = 0; count < 12 * 4; count += 4)
		edbg("Word %lu of the message is %lu\n", count, *((unsigned long *) (sep_dev->shared_area_addr + count)));
	/* update counter */
	sep_dev->host_to_sep_send_counter++;
	/* send the interrupt to SEP */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR2_REG_ADDR, sep_dev->host_to_sep_send_counter);
	/* update both counters */
	sep_dev->host_to_sep_send_counter++;
	sep_dev->sep_to_host_reply_counter++;
	dbg("SEP Driver:<-------- sep_send_reply_command_handler end\n");
}



/*
  This function handles the allocate data pool memory request
  This function returns calculates the physical address of the
  allocated memory, and the offset of this area from the mapped address.
  Therefore, the FVOs in user space can calculate the exact virtual
  address of this allocated memory
*/
static int sep_allocate_data_pool_memory_handler(unsigned long arg)
{
	int error;
	struct sep_driver_alloc_t command_args;

	dbg("SEP Driver:--------> sep_allocate_data_pool_memory_handler start\n");

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_alloc_t));
	if (error)
		goto end_function;

	/* allocate memory */
	if ((sep_dev->data_pool_bytes_allocated + command_args.num_bytes) > SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES) {
		error = -ENOTTY;
		goto end_function;
	}

	/* set the virtual and physical address */
	command_args.offset = SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES + sep_dev->data_pool_bytes_allocated;
	command_args.phys_address = sep_dev->phys_shared_area_addr + SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES + sep_dev->data_pool_bytes_allocated;

	/* write the memory back to the user space */
	error = copy_to_user((void *) arg, (void *) &command_args, sizeof(struct sep_driver_alloc_t));
	if (error)
		goto end_function;

	/* set the allocation */
	sep_dev->data_pool_bytes_allocated += command_args.num_bytes;

end_function:
	dbg("SEP Driver:<-------- sep_allocate_data_pool_memory_handler end\n");
	return error;
}

/*
  This function  handles write into allocated data pool command
*/
static int sep_write_into_data_pool_handler(unsigned long arg)
{
	int error;
	unsigned long virt_address;
	unsigned long app_in_address;
	unsigned long num_bytes;
	unsigned long data_pool_area_addr;

	dbg("SEP Driver:--------> sep_write_into_data_pool_handler start\n");

	/* get the application address */
	error = get_user(app_in_address, &(((struct sep_driver_write_t *) arg)->app_address));
	if (error)
		goto end_function;

	/* get the virtual kernel address address */
	error = get_user(virt_address, &(((struct sep_driver_write_t *) arg)->datapool_address));
	if (error)
		goto end_function;

	/* get the number of bytes */
	error = get_user(num_bytes, &(((struct sep_driver_write_t *) arg)->num_bytes));
	if (error)
		goto end_function;

	/* calculate the start of the data pool */
	data_pool_area_addr = sep_dev->shared_area_addr + SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES;


	/* check that the range of the virtual kernel address is correct */
	if ((virt_address < data_pool_area_addr) || (virt_address > (data_pool_area_addr + SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES))) {
		error = -ENOTTY;
		goto end_function;
	}
	/* copy the application data */
	error = copy_from_user((void *) virt_address, (void *) app_in_address, num_bytes);
end_function:
	dbg("SEP Driver:<-------- sep_write_into_data_pool_handler end\n");
	return error;
}

/*
  this function handles the read from data pool command
*/
static int sep_read_from_data_pool_handler(unsigned long arg)
{
	int error;
	/* virtual address of dest application buffer */
	unsigned long app_out_address;
	/* virtual address of the data pool */
	unsigned long virt_address;
	unsigned long num_bytes;
	unsigned long data_pool_area_addr;

	dbg("SEP Driver:--------> sep_read_from_data_pool_handler start\n");

	/* get the application address */
	error = get_user(app_out_address, &(((struct sep_driver_write_t *) arg)->app_address));
	if (error)
		goto end_function;

	/* get the virtual kernel address address */
	error = get_user(virt_address, &(((struct sep_driver_write_t *) arg)->datapool_address));
	if (error)
		goto end_function;

	/* get the number of bytes */
	error = get_user(num_bytes, &(((struct sep_driver_write_t *) arg)->num_bytes));
	if (error)
		goto end_function;

	/* calculate the start of the data pool */
	data_pool_area_addr = sep_dev->shared_area_addr + SEP_DRIVER_DATA_POOL_AREA_OFFSET_IN_BYTES;

	/* check that the range of the virtual kernel address is correct */
	if ((virt_address < data_pool_area_addr) || (virt_address > (data_pool_area_addr + SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES))) {
		error = -ENOTTY;
		goto end_function;
	}

	/* copy the application data */
	error = copy_to_user((void *) app_out_address, (void *) virt_address, num_bytes);
end_function:
	dbg("SEP Driver:<-------- sep_read_from_data_pool_handler end\n");
	return error;
}


/*
  this function handles tha request for creation of the DMA table
  for the synchronic symmetric operations (AES,DES)
*/
static int sep_create_sync_dma_tables_handler(unsigned long arg)
{
	int error;
	/* command arguments */
	struct sep_driver_build_sync_table_t command_args;

	dbg("SEP Driver:--------> sep_create_sync_dma_tables_handler start\n");

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_build_sync_table_t));
	if (error)
		goto end_function;

	edbg("app_in_address is %08lx\n", command_args.app_in_address);
	edbg("app_out_address is %08lx\n", command_args.app_out_address);
	edbg("data_size is %lu\n", command_args.data_in_size);
	edbg("block_size is %lu\n", command_args.block_size);

	/* check if we need to build only input table or input/output */
	if (command_args.app_out_address)
		/* prepare input and output tables */
		error = sep_prepare_input_output_dma_table(command_args.app_in_address,
							   command_args.app_out_address,
							   command_args.data_in_size,
							   command_args.block_size,
							   &command_args.in_table_address,
							   &command_args.out_table_address, &command_args.in_table_num_entries, &command_args.out_table_num_entries, &command_args.table_data_size, command_args.isKernelVirtualAddress);
	else
		/* prepare input tables */
		error = sep_prepare_input_dma_table(command_args.app_in_address,
						    command_args.data_in_size, command_args.block_size, &command_args.in_table_address, &command_args.in_table_num_entries, &command_args.table_data_size, command_args.isKernelVirtualAddress);

	if (error)
		goto end_function;
	/* copy to user */
	error = copy_to_user((void *) arg, (void *) &command_args, sizeof(struct sep_driver_build_sync_table_t));
end_function:
	dbg("SEP Driver:<-------- sep_create_sync_dma_tables_handler end\n");
	return error;
}

/*
  this function handles the request for freeing dma table for synhronic actions
*/
int sep_free_dma_table_data_handler()
{
	dbg("SEP Driver:--------> sep_free_dma_table_data_handler start\n");

	/* free input pages array */
	sep_free_dma_pages(sep_dev->in_page_array, sep_dev->in_num_pages, 0);

	/* free output pages array if needed */
	if (sep_dev->out_page_array)
		sep_free_dma_pages(sep_dev->out_page_array, sep_dev->out_num_pages, 1);

	/* reset all the values */
	sep_dev->in_page_array = 0;
	sep_dev->out_page_array = 0;
	sep_dev->in_num_pages = 0;
	sep_dev->out_num_pages = 0;
	dbg("SEP Driver:<-------- sep_free_dma_table_data_handler end\n");
	return 0;
}

/*
  this function handles the request to create the DMA tables for flow
*/
static int sep_create_flow_dma_tables_handler(unsigned long arg)
{
	int error;
	struct sep_driver_build_flow_table_t command_args;
	/* first table - output */
	struct sep_lli_entry_t first_table_data;
	/* dma table data */
	struct sep_lli_entry_t last_table_data;
	/* pointer to the info entry of the previuos DMA table */
	struct sep_lli_entry_t *prev_info_entry_ptr;
	/* pointer to the flow data strucutre */
	struct sep_flow_context_t *flow_context_ptr;

	dbg("SEP Driver:--------> sep_create_flow_dma_tables_handler start\n");

	/* init variables */
	prev_info_entry_ptr = 0;
	first_table_data.physical_address = 0xffffffff;

	/* find the free structure for flow data */
	error = sep_find_flow_context(SEP_FREE_FLOW_ID, &flow_context_ptr);
	if (error)
		goto end_function;

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_build_flow_table_t));
	if (error)
		goto end_function;

	/* create flow tables */
	error = sep_prepare_flow_dma_tables(command_args.num_virtual_buffers, command_args.virt_buff_data_addr, flow_context_ptr, &first_table_data, &last_table_data, command_args.isKernelVirtualAddress);
	if (error)
		goto end_function_with_error;

	/* check if flow is static */
	if (!command_args.flow_type)
		/* point the info entry of the last to the info entry of the first */
		last_table_data = first_table_data;

	/* set output params */
	command_args.first_table_addr = first_table_data.physical_address;
	command_args.first_table_num_entries = ((first_table_data.block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS) & SEP_NUM_ENTRIES_MASK);
	command_args.first_table_data_size = (first_table_data.block_size & SEP_TABLE_DATA_SIZE_MASK);

	/* send the parameters to user application */
	error = copy_to_user((void *) arg, &command_args, sizeof(struct sep_driver_build_flow_table_t));
	if (error)
		goto end_function_with_error;

	/* all the flow created  - update the flow entry with temp id */
	flow_context_ptr->flow_id = SEP_TEMP_FLOW_ID;

	/* set the processing tables data in the context */
	if (command_args.input_output_flag == SEP_DRIVER_IN_FLAG)
		flow_context_ptr->input_tables_in_process = first_table_data;
	else
		flow_context_ptr->output_tables_in_process = first_table_data;

	goto end_function;

end_function_with_error:
	/* free the allocated tables */
	sep_deallocated_flow_tables(&first_table_data);
end_function:
	dbg("SEP Driver:<-------- sep_create_flow_dma_tables_handler end\n");
	return error;
}

/*
  this functio n handles add tables to flow
*/
static int sep_add_flow_tables_handler(unsigned long arg)
{
	int error;
	unsigned long num_entries;
	struct sep_driver_add_flow_table_t command_args;
	struct sep_flow_context_t *flow_context_ptr;
	/* first dma table data */
	struct sep_lli_entry_t first_table_data;
	/* last dma table data */
	struct sep_lli_entry_t last_table_data;
	/* pointer to the info entry of the current DMA table */
	struct sep_lli_entry_t *info_entry_ptr;

	dbg("SEP Driver:--------> sep_add_flow_tables_handler start\n");

	/* get input parameters */
	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_add_flow_table_t));
	if (error)
		goto end_function;

	/* find the flow structure for the flow id */
	error = sep_find_flow_context(command_args.flow_id, &flow_context_ptr);
	if (error)
		goto end_function;

	/* prepare the flow dma tables */
	error = sep_prepare_flow_dma_tables(command_args.num_virtual_buffers, command_args.virt_buff_data_addr, flow_context_ptr, &first_table_data, &last_table_data, command_args.isKernelVirtualAddress);
	if (error)
		goto end_function_with_error;

	/* now check if there is already an existing add table for this flow */
	if (command_args.inputOutputFlag == SEP_DRIVER_IN_FLAG) {
		/* this buffer was for input buffers */
		if (flow_context_ptr->input_tables_flag) {
			/* add table already exists - add the new tables to the end
			   of the previous */
			num_entries = (flow_context_ptr->last_input_table.block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS) & SEP_NUM_ENTRIES_MASK;

			info_entry_ptr = (struct sep_lli_entry_t *)
			    (flow_context_ptr->last_input_table.physical_address + (sizeof(struct sep_lli_entry_t) * (num_entries - 1)));

			/* connect to list of tables */
			*info_entry_ptr = first_table_data;

			/* set the first table data */
			first_table_data = flow_context_ptr->first_input_table;
		} else {
			/* set the input flag */
			flow_context_ptr->input_tables_flag = 1;

			/* set the first table data */
			flow_context_ptr->first_input_table = first_table_data;
		}
		/* set the last table data */
		flow_context_ptr->last_input_table = last_table_data;
	} else {		/* this is output tables */

		/* this buffer was for input buffers */
		if (flow_context_ptr->output_tables_flag) {
			/* add table already exists - add the new tables to
			   the end of the previous */
			num_entries = (flow_context_ptr->last_output_table.block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS) & SEP_NUM_ENTRIES_MASK;

			info_entry_ptr = (struct sep_lli_entry_t *)
			    (flow_context_ptr->last_output_table.physical_address + (sizeof(struct sep_lli_entry_t) * (num_entries - 1)));

			/* connect to list of tables */
			*info_entry_ptr = first_table_data;

			/* set the first table data */
			first_table_data = flow_context_ptr->first_output_table;
		} else {
			/* set the input flag */
			flow_context_ptr->output_tables_flag = 1;

			/* set the first table data */
			flow_context_ptr->first_output_table = first_table_data;
		}
		/* set the last table data */
		flow_context_ptr->last_output_table = last_table_data;
	}

	/* set output params */
	command_args.first_table_addr = first_table_data.physical_address;
	command_args.first_table_num_entries = ((first_table_data.block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS) & SEP_NUM_ENTRIES_MASK);
	command_args.first_table_data_size = (first_table_data.block_size & SEP_TABLE_DATA_SIZE_MASK);

	/* send the parameters to user application */
	error = copy_to_user((void *) arg, &command_args, sizeof(struct sep_driver_add_flow_table_t));
end_function_with_error:
	/* free the allocated tables */
	sep_deallocated_flow_tables(&first_table_data);
end_function:
	dbg("SEP Driver:<-------- sep_add_flow_tables_handler end\n");
	return error;
}

/*
  this function add the flow add message to the specific flow
*/
static int sep_add_flow_tables_message_handler(unsigned long arg)
{
	int error;
	struct sep_driver_add_message_t command_args;
	struct sep_flow_context_t *flow_context_ptr;

	dbg("SEP Driver:--------> sep_add_flow_tables_message_handler start\n");

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_add_message_t));
	if (error)
		goto end_function;

	/* check input */
	if (command_args.message_size_in_bytes > SEP_MAX_ADD_MESSAGE_LENGTH_IN_BYTES) {
		error = -ENOMEM;
		goto end_function;
	}

	/* find the flow context */
	error = sep_find_flow_context(command_args.flow_id, &flow_context_ptr);
	if (error)
		goto end_function;

	/* copy the message into context */
	flow_context_ptr->message_size_in_bytes = command_args.message_size_in_bytes;
	error = copy_from_user(flow_context_ptr->message, (void *) command_args.message_address, command_args.message_size_in_bytes);
end_function:
	dbg("SEP Driver:<-------- sep_add_flow_tables_message_handler end\n");
	return error;
}


/*
  this function returns the physical and virtual addresses of the static pool
*/
static int sep_get_static_pool_addr_handler(unsigned long arg)
{
	int error;
	struct sep_driver_static_pool_addr_t command_args;

	dbg("SEP Driver:--------> sep_get_static_pool_addr_handler start\n");

	/*prepare the output parameters in the struct */
	command_args.physical_static_address = sep_dev->phys_shared_area_addr + SEP_DRIVER_STATIC_AREA_OFFSET_IN_BYTES;
	command_args.virtual_static_address = sep_dev->shared_area_addr + SEP_DRIVER_STATIC_AREA_OFFSET_IN_BYTES;

	edbg("SEP Driver:physical_static_address is %08lx, virtual_static_address %08lx\n", command_args.physical_static_address, command_args.virtual_static_address);

	/* send the parameters to user application */
	error = copy_to_user((void *) arg, &command_args, sizeof(struct sep_driver_static_pool_addr_t));
	dbg("SEP Driver:<-------- sep_get_static_pool_addr_handler end\n");
	return error;
}

/*
  this address gets the offset of the physical address from the start
  of the mapped area
*/
static int sep_get_physical_mapped_offset_handler(unsigned long arg)
{
	int error;
	struct sep_driver_get_mapped_offset_t command_args;

	dbg("SEP Driver:--------> sep_get_physical_mapped_offset_handler start\n");

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_get_mapped_offset_t));
	if (error)
		goto end_function;

	if (command_args.physical_address < sep_dev->phys_shared_area_addr) {
		error = -ENOTTY;
		goto end_function;
	}

	/*prepare the output parameters in the struct */
	command_args.offset = command_args.physical_address - sep_dev->phys_shared_area_addr;

	edbg("SEP Driver:physical_address is %08lx, offset is %lu\n", command_args.physical_address, command_args.offset);

	/* send the parameters to user application */
	error = copy_to_user((void *) arg, &command_args, sizeof(struct sep_driver_get_mapped_offset_t));
end_function:
	dbg("SEP Driver:<-------- sep_get_physical_mapped_offset_handler end\n");
	return error;
}


/*
  ?
*/
static int sep_start_handler(void)
{
	unsigned long reg_val;
	unsigned long error = 0;

	dbg("SEP Driver:--------> sep_start_handler start\n");

	/* wait in polling for message from SEP */
	do
		reg_val = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR3_REG_ADDR);
	while (!reg_val);

	/* check the value */
	if (reg_val == 0x1)
		/* fatal error - read erro status from GPRO */
		error = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR0_REG_ADDR);
end_function:
	dbg("SEP Driver:<-------- sep_start_handler end\n");
	return error;
}

/*
  this function handles the request for SEP initialization
*/
static int sep_init_handler(unsigned long arg)
{
	unsigned long message_word;
	unsigned long *message_ptr;
	struct sep_driver_init_t command_args;
	unsigned long counter;
	unsigned long error;
	unsigned long reg_val;

	dbg("SEP Driver:--------> sep_init_handler start\n");
	error = 0;

	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_init_t));

	dbg("SEP Driver:--------> sep_init_handler - finished copy_from_user \n");

	if (error)
		goto end_function;

	/* PATCH - configure the DMA to single -burst instead of multi-burst */
	/*sep_configure_dma_burst(); */

	dbg("SEP Driver:--------> sep_init_handler - finished sep_configure_dma_burst \n");

	message_ptr = (unsigned long *) command_args.message_addr;

	/* set the base address of the SRAM  */
	sep_write_reg(sep_dev, HW_SRAM_ADDR_REG_ADDR, HW_CC_SRAM_BASE_ADDRESS);

	for (counter = 0; counter < command_args.message_size_in_words; counter++, message_ptr++) {
		get_user(message_word, message_ptr);
		/* write data to SRAM */
		sep_write_reg(sep_dev, HW_SRAM_DATA_REG_ADDR, message_word);
		edbg("SEP Driver:message_word is %lu\n", message_word);
		/* wait for write complete */
		sep_wait_sram_write(sep_dev);
	}
	dbg("SEP Driver:--------> sep_init_handler - finished getting messages from user space\n");
	/* signal SEP */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR0_REG_ADDR, 0x1);

	do
		reg_val = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR3_REG_ADDR);
	while (!(reg_val & 0xFFFFFFFD));

	dbg("SEP Driver:--------> sep_init_handler - finished waiting for reg_val & 0xFFFFFFFD \n");

	/* check the value */
	if (reg_val == 0x1) {
		edbg("SEP Driver:init failed\n");

		error = sep_read_reg(sep_dev, 0x8060);
		edbg("SEP Driver:sw monitor is %lu\n", error);

		/* fatal error - read erro status from GPRO */
		error = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR0_REG_ADDR);
		edbg("SEP Driver:error is %lu\n", error);
	}
end_function:
	dbg("SEP Driver:<-------- sep_init_handler end\n");
	return error;

}

/*
  this function handles the request cache and resident reallocation
*/
static int sep_realloc_cache_resident_handler(unsigned long arg)
{
	int error;
	unsigned long phys_cache_address;
	unsigned long phys_resident_address;
	struct sep_driver_realloc_cache_resident_t command_args;

	/* copy the data */
	error = copy_from_user(&command_args, (void *) arg, sizeof(struct sep_driver_realloc_cache_resident_t));
	if (error)
		goto end_function;

	/* copy cache and resident to the their intended locations */
	error = sep_copy_cache_resident_to_area(command_args.cache_addr, command_args.cache_size_in_bytes, command_args.resident_addr, command_args.resident_size_in_bytes, &phys_cache_address, &phys_resident_address);
	if (error)
		goto end_function;

	/* lock the area (if needed) */
	sep_lock_cache_resident_area();

	command_args.new_base_addr = sep_dev->phys_shared_area_addr;

	/* find the new base address according to the lowest address between
	   cache, resident and shared area */
	if (phys_resident_address < command_args.new_base_addr)
		command_args.new_base_addr = phys_resident_address;
	if (phys_cache_address < command_args.new_base_addr)
		command_args.new_base_addr = phys_cache_address;

	/* set the return parameters */
	command_args.new_cache_addr = phys_cache_address;
	command_args.new_resident_addr = phys_resident_address;

	/* set the new shared area */
	command_args.new_shared_area_addr = sep_dev->phys_shared_area_addr;

	edbg("SEP Driver:command_args.new_shared_area_addr is %08lx\n", command_args.new_shared_area_addr);
	edbg("SEP Driver:command_args.new_base_addr is %08lx\n", command_args.new_base_addr);
	edbg("SEP Driver:command_args.new_resident_addr is %08lx\n", command_args.new_resident_addr);
	edbg("SEP Driver:command_args.new_cache_addr is %08lx\n", command_args.new_cache_addr);

	/* return to user */
	error = copy_to_user((void *) arg, (void *) &command_args, sizeof(struct sep_driver_realloc_cache_resident_t));
end_function:
	return error;
}

/*
  this function handles the request for get time
*/
static int sep_get_time_handler(unsigned long arg)
{
	int error;
	struct sep_driver_get_time_t command_args;

	error = sep_set_time(&command_args.time_physical_address, &command_args.time_value);
	error = copy_to_user((void *) arg, (void *) &command_args, sizeof(struct sep_driver_get_time_t));
	return error;

}

/*
  This api handles the setting of API mode to blocking or non-blocking
*/
static int sep_set_api_mode_handler(unsigned long arg)
{
	int error;
	unsigned long mode_flag;

	dbg("SEP Driver:--------> sep_set_api_mode_handler start\n");

	error = get_user(mode_flag, &(((struct sep_driver_set_api_mode_t *) arg)->mode));
	if (error)
		goto end_function;

	/* set the global flag */
	sep_dev->block_mode_flag = mode_flag;
end_function:
	dbg("SEP Driver:<-------- sep_set_api_mode_handler end\n");
	return error;
}

/*
  This API handles the end transaction request
*/
static int sep_end_transaction_handler(unsigned long arg)
{
	dbg("SEP Driver:--------> sep_end_transaction_handler start\n");

#if 0				/*!SEP_DRIVER_POLLING_MODE */
	/* close IMR */
	sep_write_reg(sep_dev, HW_HOST_IMR_REG_ADDR, 0x7FFF);

	/* release IRQ line */
	free_irq(SEP_DIRVER_IRQ_NUM, &sep_dev->reg_base_address);

	/* lock the sep mutex */
	mutex_unlock(&sep_mutex);
#endif

	dbg("SEP Driver:<-------- sep_end_transaction_handler end\n");

	return 0;
}

/* handler for flow done interrupt */
static void sep_flow_done_handler(struct work_struct *work)
{
	struct sep_flow_context_t *flow_data_ptr;

	/* obtain the mutex */
	mutex_lock(&sep_mutex);

	/* get the pointer to context */
	flow_data_ptr = (struct sep_flow_context_t *) work;

	/* free all the current input tables in sep */
	sep_deallocated_flow_tables(&flow_data_ptr->input_tables_in_process);

	/* free all the current tables output tables in SEP (if needed) */
	if (flow_data_ptr->output_tables_in_process.physical_address != 0xffffffff)
		sep_deallocated_flow_tables(&flow_data_ptr->output_tables_in_process);

	/* check if we have additional tables to be sent to SEP only input
	   flag may be checked */
	if (flow_data_ptr->input_tables_flag) {
		/* copy the message to the shared RAM and signal SEP */
		memcpy((void *) flow_data_ptr->message, (void *) sep_dev->shared_area_addr, flow_data_ptr->message_size_in_bytes);

		sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR2_REG_ADDR, 0x2);
	}
	mutex_unlock(&sep_mutex);
}


/*
  This function creates a list of tables for flow and returns the data for
	the first and last tables of the list
*/
static int sep_prepare_flow_dma_tables(unsigned long num_virtual_buffers,
				       unsigned long first_buff_addr, struct sep_flow_context_t *flow_data_ptr, struct sep_lli_entry_t *first_table_data_ptr, struct sep_lli_entry_t *last_table_data_ptr, bool isKernelVirtualAddress)
{
	int error;
	unsigned long virt_buff_addr;
	unsigned long virt_buff_size;
	struct sep_lli_entry_t table_data;
	struct sep_lli_entry_t *info_entry_ptr;
	struct sep_lli_entry_t *prev_info_entry_ptr;
	unsigned long i;

	/* init vars */
	error = 0;
	prev_info_entry_ptr = 0;

	/* init the first table to default */
	table_data.physical_address = 0xffffffff;
	first_table_data_ptr->physical_address = 0xffffffff;
	table_data.block_size = 0;

	for (i = 0; i < num_virtual_buffers; i++) {
		/* get the virtual buffer address */
		error = get_user(virt_buff_addr, &first_buff_addr);
		if (error)
			goto end_function;

		/* get the virtual buffer size */
		first_buff_addr++;
		error = get_user(virt_buff_size, &first_buff_addr);
		if (error)
			goto end_function;

		/* advance the address to point to the next pair of address|size */
		first_buff_addr++;

		/* now prepare the one flow LLI table from the data */
		error = sep_prepare_one_flow_dma_table(virt_buff_addr, virt_buff_size, &table_data, &info_entry_ptr, flow_data_ptr, isKernelVirtualAddress);
		if (error)
			goto end_function;

		if (i == 0) {
			/* if this is the first table - save it to return to the user
			   application */
			*first_table_data_ptr = table_data;

			/* set the pointer to info entry */
			prev_info_entry_ptr = info_entry_ptr;
		} else {
			/* not first table - the previous table info entry should
			   be updated */
			prev_info_entry_ptr->block_size = (0x1 << SEP_INT_FLAG_OFFSET_IN_BITS) | (table_data.block_size);

			/* set the pointer to info entry */
			prev_info_entry_ptr = info_entry_ptr;
		}
	}

	/* set the last table data */
	*last_table_data_ptr = table_data;
end_function:
	return error;
}


/*
  This function creates one DMA table for flow and returns its data,
  and pointer to its info entry
*/
static int sep_prepare_one_flow_dma_table(unsigned long virt_buff_addr, unsigned long virt_buff_size, struct sep_lli_entry_t *table_data, struct sep_lli_entry_t **info_entry_ptr, struct sep_flow_context_t *flow_data_ptr, bool isKernelVirtualAddress)
{
	int error;
	/* the range in pages */
	unsigned long lli_array_size;
	struct sep_lli_entry_t *lli_array;
	struct sep_lli_entry_t *flow_dma_table_entry_ptr;
	unsigned long *start_dma_table_ptr;
	/* total table data counter */
	unsigned long dma_table_data_count;
	/* pointer that will keep the pointer to the pages of the virtual buffer */
	struct page **page_array_ptr;
	unsigned long entry_count;

	/* find the space for the new table */
	error = sep_find_free_flow_dma_table_space(&start_dma_table_ptr);
	if (error)
		goto end_function;

	/* check if the pages are in Kernel Virtual Address layout */
	if (isKernelVirtualAddress == true)
		/* lock kernel buffer in the memory */
		error = sep_lock_kernel_pages(virt_buff_addr, virt_buff_size, &lli_array_size, &lli_array, &page_array_ptr);
	else
		/* lock user buffer in the memory */
		error = sep_lock_user_pages(virt_buff_addr, virt_buff_size, &lli_array_size, &lli_array, &page_array_ptr);

	if (error)
		goto end_function;

	/* set the pointer to page array at the beginning of table - this table is
	   now considered taken */
	*start_dma_table_ptr = lli_array_size;

	/* point to the place of the pages pointers of the table */
	start_dma_table_ptr++;

	/* set the pages pointer */
	*start_dma_table_ptr = (unsigned long) page_array_ptr;

	/* set the pointer to the first entry */
	flow_dma_table_entry_ptr = (struct sep_lli_entry_t *) (++start_dma_table_ptr);

	/* now create the entries for table */
	for (dma_table_data_count = entry_count = 0; entry_count < lli_array_size; entry_count++) {
		flow_dma_table_entry_ptr->physical_address = lli_array[entry_count].physical_address;

		flow_dma_table_entry_ptr->block_size = lli_array[entry_count].block_size;

		/* set the total data of a table */
		dma_table_data_count += lli_array[entry_count].block_size;

		flow_dma_table_entry_ptr++;
	}

	/* set the physical address */
	table_data->physical_address = virt_to_phys(start_dma_table_ptr);

	/* set the num_entries and total data size */
	table_data->block_size = ((lli_array_size + 1) << SEP_NUM_ENTRIES_OFFSET_IN_BITS) | (dma_table_data_count);

	/* set the info entry */
	flow_dma_table_entry_ptr->physical_address = 0xffffffff;
	flow_dma_table_entry_ptr->block_size = 0;

	/* set the pointer to info entry */
	*info_entry_ptr = flow_dma_table_entry_ptr;

	/* the array of the lli entries */
	kfree(lli_array);
end_function:
	return error;
}


/*
  This function returns pointer to the  flow data structure
  that contains the given id
*/
static int sep_find_flow_context(unsigned long flow_id, struct sep_flow_context_t **flow_data_ptr)
{
	unsigned long count;
	int error = 0;

	/*
	   always search for flow with id default first - in case we
	   already started working on the flow there can be no situation
	   when 2 flows are with default flag
	 */
	for (count = 0; count < SEP_DRIVER_NUM_FLOWS; count++) {
		if (sep_dev->flows_data_array[count].flow_id == flow_id) {
			*flow_data_ptr = &sep_dev->flows_data_array[count];
			break;
		}
	}

	if (count == SEP_DRIVER_NUM_FLOWS)
		/* no flow found  */
		error = -ENOMEM;

	return error;
}

/*
  this function find a space for the new flow dma table
*/
static int sep_find_free_flow_dma_table_space(unsigned long **table_address_ptr)
{
	int error = 0;
	/* pointer to the id field of the flow dma table */
	unsigned long *start_table_ptr;
	unsigned long flow_dma_area_start_addr;
	unsigned long flow_dma_area_end_addr;
	/* maximum table size in words */
	unsigned long table_size_in_words;

	/* find the start address of the flow DMA table area */
	flow_dma_area_start_addr = sep_dev->shared_area_addr + SEP_DRIVER_FLOW_DMA_TABLES_AREA_OFFSET_IN_BYTES;

	/* set end address of the flow table area */
	flow_dma_area_end_addr = flow_dma_area_start_addr + SEP_DRIVER_FLOW_DMA_TABLES_AREA_SIZE_IN_BYTES;

	/* set table size in words */
	table_size_in_words = SEP_DRIVER_MAX_FLOW_NUM_ENTRIES_IN_TABLE * (sizeof(struct sep_lli_entry_t) / sizeof(long)) + 2;

	/* set the pointer to the start address of DMA area */
	start_table_ptr = (unsigned long *) flow_dma_area_start_addr;

	/* find the space for the next table */
	while (((*start_table_ptr & 0x7FFFFFFF) != 0) && ((unsigned long) start_table_ptr < flow_dma_area_end_addr))
		start_table_ptr += table_size_in_words;

	/* check if we reached the end of floa tables area */
	if ((unsigned long) start_table_ptr >= flow_dma_area_end_addr)
		error = -1;
	else
		*table_address_ptr = start_table_ptr;

	return error;
}

/*
  this function goes over all the flow tables connected to the given
	table and deallocate them
*/
static void sep_deallocated_flow_tables(struct sep_lli_entry_t *first_table_ptr)
{
	/* id pointer */
	unsigned long *table_ptr;
	/* end address of the flow dma area */
	unsigned long num_entries;
	unsigned long num_pages;
	struct page **pages_ptr;
	/* maximum table size in words */
	struct sep_lli_entry_t *info_entry_ptr;

	/* set the pointer to the first table */
	table_ptr = (unsigned long *) first_table_ptr->physical_address;

	/* set the num of entries */
	num_entries = (first_table_ptr->block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS)
	    & SEP_NUM_ENTRIES_MASK;

	/* go over all the connected tables */
	while (*table_ptr != 0xffffffff) {
		/* get number of pages */
		num_pages = *(table_ptr - 2);

		/* get the pointer to the pages */
		pages_ptr = (struct page **) (*(table_ptr - 1));

		/* free the pages */
		sep_free_dma_pages(pages_ptr, num_pages, 1);

		/* goto to the info entry */
		info_entry_ptr = ((struct sep_lli_entry_t *) table_ptr) + (num_entries - 1);

		table_ptr = (unsigned long *) info_entry_ptr->physical_address;
		num_entries = (info_entry_ptr->block_size >> SEP_NUM_ENTRIES_OFFSET_IN_BITS) & SEP_NUM_ENTRIES_MASK;
	}

	return;
}

/*
  This function handler the set flow id command
*/
static int sep_set_flow_id_handler(unsigned long arg)
{
	int error;
	unsigned long flow_id;
	struct sep_flow_context_t *flow_data_ptr;

	dbg("------------>SEP Driver: sep_set_flow_id_handler start\n");

	error = get_user(flow_id, &(((struct sep_driver_set_flow_id_t *) arg)->flow_id));
	if (error)
		goto end_function;

	/* find the flow data structure that was just used for creating new flow
	   - its id should be default */
	error = sep_find_flow_context(SEP_TEMP_FLOW_ID, &flow_data_ptr);
	if (error)
		goto end_function;

	/* set flow id */
	flow_data_ptr->flow_id = flow_id;

end_function:
	dbg("SEP Driver:<-------- sep_set_flow_id_handler end\n");
	return error;
}


/*
  calculates time and sets it at the predefined address
*/
static int sep_set_time(unsigned long *address_ptr, unsigned long *time_in_sec_ptr)
{
	struct timeval time;
	/* address of time in the kernel */
	unsigned long time_addr;


	dbg("SEP Driver:--------> sep_set_time start\n");

	do_gettimeofday(&time);

	/* set value in the SYSTEM MEMORY offset */
	time_addr = sep_dev->message_shared_area_addr + SEP_DRIVER_SYSTEM_TIME_MEMORY_OFFSET_IN_BYTES;

	*(unsigned long *) time_addr = SEP_TIME_VAL_TOKEN;
	*(unsigned long *) (time_addr + 4) = time.tv_sec;

	edbg("SEP Driver:time.tv_sec is %lu\n", time.tv_sec);
	edbg("SEP Driver:time_addr is %lu\n", time_addr);
	edbg("SEP Driver:g_message_shared_area_addr is %lu\n", sep_dev->message_shared_area_addr);

	/* set the output parameters if needed */
	if (address_ptr)
		*address_ptr = sep_shared_area_virt_to_phys(time_addr);

	if (time_in_sec_ptr)
		*time_in_sec_ptr = time.tv_sec;

	dbg("SEP Driver:<-------- sep_set_time end\n");

	return 0;
}

static void sep_wait_busy(struct sep_device *dev)
{
	u32 reg;

	do {
		reg = sep_read_reg(sep_dev, HW_HOST_SEP_BUSY_REG_ADDR);
	} while (reg);
}

/*
  PATCH for configuring the DMA to single burst instead of multi-burst
*/
static void sep_configure_dma_burst(void)
{
#define 	 HW_AHB_RD_WR_BURSTS_REG_ADDR 		 0x0E10UL

	dbg("SEP Driver:<-------- sep_configure_dma_burst start \n");

	/* request access to registers from SEP */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR0_REG_ADDR, 0x2);

	dbg("SEP Driver:<-------- sep_configure_dma_burst finished request access to registers from SEP (write reg)  \n");

	sep_wait_busy(sep_dev);

	dbg("SEP Driver:<-------- sep_configure_dma_burst finished request access to registers from SEP (while(revVal) wait loop)  \n");

	/* set the DMA burst register to single burst */
	sep_write_reg(sep_dev, HW_AHB_RD_WR_BURSTS_REG_ADDR, 0x0UL);

	/* release the sep busy */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR0_REG_ADDR, 0x0UL);
	sep_wait_busy(sep_dev);

	dbg("SEP Driver:<-------- sep_configure_dma_burst done  \n");

}

/* major and minor device numbers */
static dev_t sep_devno;

/* the files operations structure of the driver */
static struct file_operations sep_file_operations = {
	.owner = THIS_MODULE,
	.ioctl = sep_ioctl,
	.poll = sep_poll,
	.open = sep_open,
	.release = sep_release,
	.mmap = sep_mmap,
};


/* cdev struct of the driver */
static struct cdev sep_cdev;

/*
  this function registers the driver to the file system
*/
static int sep_register_driver_to_fs(void)
{
	int ret_val = alloc_chrdev_region(&sep_devno, 0, 1, "sep_sec_driver");
	if (ret_val) {
		edbg("sep_driver:major number allocation failed, retval is %d\n", ret_val);
		goto end_function;
	}

	/* init cdev */
	cdev_init(&sep_cdev, &sep_file_operations);
	sep_cdev.owner = THIS_MODULE;

	/* register the driver with the kernel */
	ret_val = cdev_add(&sep_cdev, sep_devno, 1);

	if (ret_val) {
		edbg("sep_driver:cdev_add failed, retval is %d\n", ret_val);
		goto end_function_unregister_devnum;
	}

	goto end_function;

end_function_unregister_devnum:

	/* unregister dev numbers */
	unregister_chrdev_region(sep_devno, 1);

end_function:
      return ret_val;
}

/*
  this function unregisters driver from fs
*/
static void sep_unregister_driver_from_fs(void)
{
	cdev_del(&sep_cdev);
	/* unregister dev numbers */
	unregister_chrdev_region(sep_devno, 1);
}


/*--------------------------------------------------------------
  init function
----------------------------------------------------------------*/
static int __init sep_init(void)
{
	int ret_val = 0;
	int counter;
	int size;		/* size of memory for allocation */

	dbg("SEP Driver:-------->Init start\n");
	edbg("sep->shared_area_addr = %lx\n", (unsigned long) &sep_dev->shared_area_addr);

	/* transaction counter that coordinates the transactions between SEP
	and HOST */
	sep_dev->host_to_sep_send_counter = 0;

	/* counter for the messages from sep */
	sep_dev->sep_to_host_reply_counter = 0;

	/* counter for the number of bytes allocated in the pool
	for the current transaction */
	sep_dev->data_pool_bytes_allocated = 0;

	/* set the starting mode to blocking */
	sep_dev->block_mode_flag = 1;

	ret_val = sep_register_driver_to_device();
	if (ret_val) {
		edbg("sep_driver:sep_driver_to_device failed, ret_val is %d\n", ret_val);
		goto end_function_unregister_from_fs;
	}
	/* calculate the total size for allocation */
	size = SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES +
	    SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_SIZE_IN_BYTES + SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES + SEP_DRIVER_FLOW_DMA_TABLES_AREA_SIZE_IN_BYTES + SEP_DRIVER_STATIC_AREA_SIZE_IN_BYTES + SEP_DRIVER_SYSTEM_DATA_MEMORY_SIZE_IN_BYTES;

	/* allocate the shared area */
	if (sep_map_and_alloc_shared_area(size, &sep_dev->shared_area_addr, &sep_dev->phys_shared_area_addr)) {
		ret_val = -ENOMEM;
		/* allocation failed */
		goto end_function_unmap_io_memory;
	}
	/* now set the memory regions */
	sep_dev->message_shared_area_addr = sep_dev->shared_area_addr;

	edbg("SEP Driver: g_message_shared_area_addr is %08lx\n", sep_dev->message_shared_area_addr);

#if (SEP_DRIVER_RECONFIG_MESSAGE_AREA == 1)
	/* send the new SHARED MESSAGE AREA to the SEP */
	sep_write_reg(sep_dev, HW_HOST_HOST_SEP_GPR1_REG_ADDR, sep_dev->phys_shared_area_addr);

	/* poll for SEP response */
	retVal = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR1_REG_ADDR);
	while (retVal != 0xffffffff && retVal != sep_dev->phys_shared_area_addr)
		retVal = sep_read_reg(sep_dev, HW_HOST_SEP_HOST_GPR1_REG_ADDR);

	/* check the return value (register) */
	if (retVal != sep_dev->phys_shared_area_addr) {
		ret_val = -ENOMEM;
		goto end_function_deallocate_message_area;
	}
#endif
	/* init the flow contextes */
	for (counter = 0; counter < SEP_DRIVER_NUM_FLOWS; counter++)
		sep_dev->flows_data_array[counter].flow_id = SEP_FREE_FLOW_ID;

	sep_dev->flow_wq_ptr = create_singlethread_workqueue("sepflowwq");
	if (sep_dev->flow_wq_ptr == 0) {
		ret_val = -ENOMEM;
		edbg("sep_driver:flow queue creation failed\n");
		goto end_function_deallocate_sep_shared_area;
	}
	edbg("SEP Driver: create flow workqueue \n");

	/* register driver to fs */
	ret_val = sep_register_driver_to_fs();
	if (ret_val)
		goto end_function_deallocate_sep_shared_area;
	/* load the rom code */
	sep_load_rom_code();
	goto end_function;
end_function_unregister_from_fs:
	/* unregister from fs */
	sep_unregister_driver_from_fs();
end_function_deallocate_sep_shared_area:
	/* de-allocate shared area */
	sep_unmap_and_free_shared_area(size, sep_dev->shared_area_addr, sep_dev->phys_shared_area_addr);
end_function_unmap_io_memory:
	iounmap((void *) sep_dev->reg_base_address);
	/* release io memory region */
	release_mem_region(SEP_IO_MEM_REGION_START_ADDRESS, SEP_IO_MEM_REGION_SIZE);
end_function:
	dbg("SEP Driver:<-------- Init end\n");
	return ret_val;
}


/*-------------------------------------------------------------
  exit function
--------------------------------------------------------------*/
static void __exit sep_exit(void)
{
	int size;

	dbg("SEP Driver:--------> Exit start\n");

	/* unregister from fs */
	sep_unregister_driver_from_fs();
	/* calculate the total size for de-allocation */
	size = SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES +
	    SEP_DRIVER_SYNCHRONIC_DMA_TABLES_AREA_SIZE_IN_BYTES + SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES + SEP_DRIVER_FLOW_DMA_TABLES_AREA_SIZE_IN_BYTES + SEP_DRIVER_STATIC_AREA_SIZE_IN_BYTES + SEP_DRIVER_SYSTEM_DATA_MEMORY_SIZE_IN_BYTES;
	/* free shared area  */
	sep_unmap_and_free_shared_area(size, sep_dev->shared_area_addr, sep_dev->phys_shared_area_addr);
	edbg("SEP Driver: free pages SEP SHARED AREA \n");
	iounmap((void *) sep_dev->reg_base_address);
	edbg("SEP Driver: iounmap \n");
	/* release io memory region */
	release_mem_region(SEP_IO_MEM_REGION_START_ADDRESS, SEP_IO_MEM_REGION_SIZE);
	edbg("SEP Driver: release_mem_region \n");
	dbg("SEP Driver:<-------- Exit end\n");
}


module_init(sep_init);
module_exit(sep_exit);

MODULE_LICENSE("GPL");
