/*
 *
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *   Hank Janssen  <hjanssen@microsoft.com>
 *
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include "StorVsc.c"

static const char* gBlkDriverName="blkvsc";

/* {32412632-86cb-44a2-9b5c-50d1417354f5} */
static const GUID gBlkVscDeviceType={
	.Data = {0x32, 0x26, 0x41, 0x32, 0xcb, 0x86, 0xa2, 0x44, 0x9b, 0x5c, 0x50, 0xd1, 0x41, 0x73, 0x54, 0xf5}
};

/* Static routines */
static int
BlkVscOnDeviceAdd(
	struct hv_device *Device,
	void			*AdditionalInfo
	);


int
BlkVscInitialize(
	struct hv_driver *Driver
	)
{
	STORVSC_DRIVER_OBJECT* storDriver = (STORVSC_DRIVER_OBJECT*)Driver;
	int ret=0;

	DPRINT_ENTER(BLKVSC);

	/* Make sure we are at least 2 pages since 1 page is used for control */
	ASSERT(storDriver->RingBufferSize >= (PAGE_SIZE << 1));

	Driver->name = gBlkDriverName;
	memcpy(&Driver->deviceType, &gBlkVscDeviceType, sizeof(GUID));

	storDriver->RequestExtSize			= sizeof(STORVSC_REQUEST_EXTENSION);
	/* Divide the ring buffer data size (which is 1 page less than the ring buffer size since that page is reserved for the ring buffer indices) */
	/* by the max request size (which is VMBUS_CHANNEL_PACKET_MULITPAGE_BUFFER + VSTOR_PACKET + u64) */
	storDriver->MaxOutstandingRequestsPerChannel =
		((storDriver->RingBufferSize - PAGE_SIZE) / ALIGN_UP(MAX_MULTIPAGE_BUFFER_PACKET + sizeof(VSTOR_PACKET) + sizeof(u64),sizeof(u64)));

	DPRINT_INFO(BLKVSC, "max io outstd %u", storDriver->MaxOutstandingRequestsPerChannel);

	/* Setup the dispatch table */
	storDriver->Base.OnDeviceAdd		= BlkVscOnDeviceAdd;
	storDriver->Base.OnDeviceRemove		= StorVscOnDeviceRemove;
	storDriver->Base.OnCleanup			= StorVscOnCleanup;

	storDriver->OnIORequest				= StorVscOnIORequest;

	DPRINT_EXIT(BLKVSC);

	return ret;
}

int
BlkVscOnDeviceAdd(
	struct hv_device *Device,
	void			*AdditionalInfo
	)
{
	int ret=0;
	STORVSC_DEVICE_INFO *deviceInfo = (STORVSC_DEVICE_INFO*)AdditionalInfo;

	DPRINT_ENTER(BLKVSC);

	ret = StorVscOnDeviceAdd(Device, AdditionalInfo);

	if (ret != 0)
	{
		DPRINT_EXIT(BLKVSC);

		return ret;
	}

	/* We need to use the device instance guid to set the path and target id. For IDE devices, the */
	/* device instance id is formatted as <bus id> - <device id> - 8899 - 000000000000. */
	deviceInfo->PathId = Device->deviceInstance.Data[3] << 24 | Device->deviceInstance.Data[2] << 16 |
		Device->deviceInstance.Data[1] << 8 |Device->deviceInstance.Data[0];

	deviceInfo->TargetId = Device->deviceInstance.Data[5] << 8 | Device->deviceInstance.Data[4];

	DPRINT_EXIT(BLKVSC);

	return ret;
}
