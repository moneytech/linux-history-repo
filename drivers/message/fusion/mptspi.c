/*
 *  linux/drivers/message/fusion/mptspi.c
 *      For use with LSI Logic PCI chip/adapter(s)
 *      running LSI Logic Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2005 LSI Logic Corporation
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include "linux_compat.h"	/* linux-2.6 tweaks */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>	/* for mdelay */
#include <linux/interrupt.h>	/* needed for in_interrupt() proto */
#include <linux/reboot.h>	/* notifier code */
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/raid_class.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

#include "mptbase.h"
#include "mptscsih.h"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#define my_NAME		"Fusion MPT SPI Host driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptspi"

MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");

/* Command line args */
static int mpt_saf_te = MPTSCSIH_SAF_TE;
module_param(mpt_saf_te, int, 0);
MODULE_PARM_DESC(mpt_saf_te, " Force enabling SEP Processor: enable=1  (default=MPTSCSIH_SAF_TE=0)");

static int mpt_pq_filter = 0;
module_param(mpt_pq_filter, int, 0);
MODULE_PARM_DESC(mpt_pq_filter, " Enable peripheral qualifier filter: enable=1  (default=0)");

static void mptspi_write_offset(struct scsi_target *, int);
static void mptspi_write_width(struct scsi_target *, int);
static int mptspi_write_spi_device_pg1(struct scsi_target *,
				       struct _CONFIG_PAGE_SCSI_DEVICE_1 *);

static struct scsi_transport_template *mptspi_transport_template = NULL;

static int	mptspiDoneCtx = -1;
static int	mptspiTaskCtx = -1;
static int	mptspiInternalCtx = -1; /* Used only for internal commands */

static int mptspi_target_alloc(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)shost->hostdata;
	int ret;

	if (hd == NULL)
		return -ENODEV;

	ret = mptscsih_target_alloc(starget);
	if (ret)
		return ret;

	/* if we're a device on virtual channel 1 and we're not part
	 * of an array, just return here (otherwise the setup below
	 * may actually affect a real physical device on channel 0 */
	if (starget->channel == 1 &&
	    mptscsih_raid_id_to_num(hd, starget->id) < 0)
		return 0;

	if (hd->ioc->spi_data.nvram &&
	    hd->ioc->spi_data.nvram[starget->id] != MPT_HOST_NVRAM_INVALID) {
		u32 nvram = hd->ioc->spi_data.nvram[starget->id];
		spi_min_period(starget) = (nvram & MPT_NVRAM_SYNC_MASK) >> MPT_NVRAM_SYNC_SHIFT;
		spi_max_width(starget) = nvram & MPT_NVRAM_WIDE_DISABLE ? 0 : 1;
	} else {
		spi_min_period(starget) = hd->ioc->spi_data.minSyncFactor;
		spi_max_width(starget) = hd->ioc->spi_data.maxBusWidth;
	}
	spi_max_offset(starget) = hd->ioc->spi_data.maxSyncOffset;

	spi_offset(starget) = 0;
	mptspi_write_width(starget, 0);

	return 0;
}

static int mptspi_read_spi_device_pg0(struct scsi_target *starget,
			     struct _CONFIG_PAGE_SCSI_DEVICE_0 *pass_pg0)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)shost->hostdata;
	struct _MPT_ADAPTER *ioc = hd->ioc;
	struct _CONFIG_PAGE_SCSI_DEVICE_0 *pg0;
	dma_addr_t pg0_dma;
	int size;
	struct _x_config_parms cfg;
	struct _CONFIG_PAGE_HEADER hdr;
	int err = -EBUSY;

	/* No SPI parameters for RAID devices */
	if (starget->channel == 0 &&
	    (hd->ioc->raid_data.isRaid & (1 << starget->id)))
		return -1;

	size = ioc->spi_data.sdp0length * 4;
	/*
	if (ioc->spi_data.sdp0length & 1)
		size += size + 4;
	size += 2048;
	*/

	pg0 = dma_alloc_coherent(&ioc->pcidev->dev, size, &pg0_dma, GFP_KERNEL);
	if (pg0 == NULL) {
		starget_printk(KERN_ERR, starget, "dma_alloc_coherent for parameters failed\n");
		return -EINVAL;
	}

	memset(&hdr, 0, sizeof(hdr));

	hdr.PageVersion = ioc->spi_data.sdp0version;
	hdr.PageLength = ioc->spi_data.sdp0length;
	hdr.PageNumber = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

	memset(&cfg, 0, sizeof(cfg));

	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = pg0_dma;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfg.dir = 0;
	cfg.pageAddr = starget->id;

	if (mpt_config(ioc, &cfg)) {
		starget_printk(KERN_ERR, starget, "mpt_config failed\n");
		goto out_free;
	}
	err = 0;
	memcpy(pass_pg0, pg0, size);

 out_free:
	dma_free_coherent(&ioc->pcidev->dev, size, pg0, pg0_dma);
	return err;
}

static u32 mptspi_getRP(struct scsi_target *starget)
{
	u32 nego = 0;

	nego |= spi_iu(starget) ? MPI_SCSIDEVPAGE1_RP_IU : 0;
	nego |= spi_dt(starget) ? MPI_SCSIDEVPAGE1_RP_DT : 0;
	nego |= spi_qas(starget) ? MPI_SCSIDEVPAGE1_RP_QAS : 0;
	nego |= spi_hold_mcs(starget) ? MPI_SCSIDEVPAGE1_RP_HOLD_MCS : 0;
	nego |= spi_wr_flow(starget) ? MPI_SCSIDEVPAGE1_RP_WR_FLOW : 0;
	nego |= spi_rd_strm(starget) ? MPI_SCSIDEVPAGE1_RP_RD_STRM : 0;
	nego |= spi_rti(starget) ? MPI_SCSIDEVPAGE1_RP_RTI : 0;
	nego |= spi_pcomp_en(starget) ? MPI_SCSIDEVPAGE1_RP_PCOMP_EN : 0;

	nego |= (spi_period(starget) <<  MPI_SCSIDEVPAGE1_RP_SHIFT_MIN_SYNC_PERIOD) & MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK;
	nego |= (spi_offset(starget) << MPI_SCSIDEVPAGE1_RP_SHIFT_MAX_SYNC_OFFSET) & MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK;
	nego |= spi_width(starget) ?  MPI_SCSIDEVPAGE1_RP_WIDE : 0;

	return nego;
}

static void mptspi_read_parameters(struct scsi_target *starget)
{
	int nego;
	struct _CONFIG_PAGE_SCSI_DEVICE_0 pg0;

	mptspi_read_spi_device_pg0(starget, &pg0);

	nego = le32_to_cpu(pg0.NegotiatedParameters);

	spi_iu(starget) = (nego & MPI_SCSIDEVPAGE0_NP_IU) ? 1 : 0;
	spi_dt(starget) = (nego & MPI_SCSIDEVPAGE0_NP_DT) ? 1 : 0;
	spi_qas(starget) = (nego & MPI_SCSIDEVPAGE0_NP_QAS) ? 1 : 0;
	spi_wr_flow(starget) = (nego & MPI_SCSIDEVPAGE0_NP_WR_FLOW) ? 1 : 0;
	spi_rd_strm(starget) = (nego & MPI_SCSIDEVPAGE0_NP_RD_STRM) ? 1 : 0;
	spi_rti(starget) = (nego & MPI_SCSIDEVPAGE0_NP_RTI) ? 1 : 0;
	spi_pcomp_en(starget) = (nego & MPI_SCSIDEVPAGE0_NP_PCOMP_EN) ? 1 : 0;
	spi_hold_mcs(starget) = (nego & MPI_SCSIDEVPAGE0_NP_HOLD_MCS) ? 1 : 0;
	spi_period(starget) = (nego & MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK) >> MPI_SCSIDEVPAGE0_NP_SHIFT_SYNC_PERIOD;
	spi_offset(starget) = (nego & MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK) >> MPI_SCSIDEVPAGE0_NP_SHIFT_SYNC_OFFSET;
	spi_width(starget) = (nego & MPI_SCSIDEVPAGE0_NP_WIDE) ? 1 : 0;
}

static int
mptscsih_quiesce_raid(MPT_SCSI_HOST *hd, int quiesce, int disk)
{
	MpiRaidActionRequest_t	*pReq;
	MPT_FRAME_HDR		*mf;

	/* Get and Populate a free Frame
	 */
	if ((mf = mpt_get_msg_frame(hd->ioc->InternalCtx, hd->ioc)) == NULL) {
		ddvprintk((MYIOC_s_WARN_FMT "_do_raid: no msg frames!\n",
					hd->ioc->name));
		return -EAGAIN;
	}
	pReq = (MpiRaidActionRequest_t *)mf;
	if (quiesce)
		pReq->Action = MPI_RAID_ACTION_QUIESCE_PHYS_IO;
	else
		pReq->Action = MPI_RAID_ACTION_ENABLE_PHYS_IO;
	pReq->Reserved1 = 0;
	pReq->ChainOffset = 0;
	pReq->Function = MPI_FUNCTION_RAID_ACTION;
	pReq->VolumeID = disk;
	pReq->VolumeBus = 0;
	pReq->PhysDiskNum = 0;
	pReq->MsgFlags = 0;
	pReq->Reserved2 = 0;
	pReq->ActionDataWord = 0; /* Reserved for this action */

	mpt_add_sge((char *)&pReq->ActionDataSGE,
		MPT_SGE_FLAGS_SSIMPLE_READ | 0, (dma_addr_t) -1);

	ddvprintk((MYIOC_s_INFO_FMT "RAID Volume action %x id %d\n",
			hd->ioc->name, action, io->id));

	hd->pLocal = NULL;
	hd->timer.expires = jiffies + HZ*10; /* 10 second timeout */
	hd->scandv_wait_done = 0;

	/* Save cmd pointer, for resource free if timeout or
	 * FW reload occurs
	 */
	hd->cmdPtr = mf;

	add_timer(&hd->timer);
	mpt_put_msg_frame(hd->ioc->InternalCtx, hd->ioc, mf);
	wait_event(hd->scandv_waitq, hd->scandv_wait_done);

	if ((hd->pLocal == NULL) || (hd->pLocal->completion != 0))
		return -1;

	return 0;
}

static void mptspi_dv_device(struct _MPT_SCSI_HOST *hd,
			     struct scsi_device *sdev)
{
	VirtTarget *vtarget = scsi_target(sdev)->hostdata;

	/* no DV on RAID devices */
	if (sdev->channel == 0 &&
	    (hd->ioc->raid_data.isRaid & (1 << sdev->id)))
		return;

	/* If this is a piece of a RAID, then quiesce first */
	if (sdev->channel == 1 &&
	    mptscsih_quiesce_raid(hd, 1, vtarget->target_id) < 0) {
		starget_printk(KERN_ERR, scsi_target(sdev),
			       "Integrated RAID quiesce failed\n");
		return;
	}

	spi_dv_device(sdev);

	if (sdev->channel == 1 &&
	    mptscsih_quiesce_raid(hd, 0, vtarget->target_id) < 0)
		starget_printk(KERN_ERR, scsi_target(sdev),
			       "Integrated RAID resume failed\n");

	mptspi_read_parameters(sdev->sdev_target);
	spi_display_xfer_agreement(sdev->sdev_target);
	mptspi_read_parameters(sdev->sdev_target);
}

static int mptspi_slave_alloc(struct scsi_device *sdev)
{
	int ret;
	MPT_SCSI_HOST *hd = (MPT_SCSI_HOST *)sdev->host->hostdata;
	/* gcc doesn't see that all uses of this variable occur within
	 * the if() statements, so stop it from whining */
	int physdisknum = 0;

	if (sdev->channel == 1) {
		physdisknum = mptscsih_raid_id_to_num(hd, sdev->id);

		if (physdisknum < 0)
			return physdisknum;
	}

	ret = mptscsih_slave_alloc(sdev);

	if (ret)
		return ret;

	if (sdev->channel == 1) {
		VirtDevice *vdev = sdev->hostdata;
		sdev->no_uld_attach = 1;
		vdev->vtarget->tflags |= MPT_TARGET_FLAGS_RAID_COMPONENT;
		/* The real channel for this device is zero */
		vdev->vtarget->bus_id = 0;
		/* The actual physdisknum (for RAID passthrough) */
		vdev->vtarget->target_id = physdisknum;
	}

	return 0;
}

static int mptspi_slave_configure(struct scsi_device *sdev)
{
	int ret = mptscsih_slave_configure(sdev);
	struct _MPT_SCSI_HOST *hd =
		(struct _MPT_SCSI_HOST *)sdev->host->hostdata;

	if (ret)
		return ret;

	if ((sdev->channel == 1 ||
	     !(hd->ioc->raid_data.isRaid & (1 << sdev->id))) &&
	    !spi_initial_dv(sdev->sdev_target))
		mptspi_dv_device(hd, sdev);

	return 0;
}

static void mptspi_slave_destroy(struct scsi_device *sdev)
{
	struct scsi_target *starget = scsi_target(sdev);
	VirtTarget *vtarget = starget->hostdata;
	VirtDevice *vdevice = sdev->hostdata;

	/* Will this be the last lun on a non-raid device? */
	if (vtarget->num_luns == 1 && vdevice->configured_lun) {
		struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;

		/* Async Narrow */
		pg1.RequestedParameters = 0;
		pg1.Reserved = 0;
		pg1.Configuration = 0;

		mptspi_write_spi_device_pg1(starget, &pg1);
	}

	mptscsih_slave_destroy(sdev);
}

static struct scsi_host_template mptspi_driver_template = {
	.module				= THIS_MODULE,
	.proc_name			= "mptspi",
	.proc_info			= mptscsih_proc_info,
	.name				= "MPT SPI Host",
	.info				= mptscsih_info,
	.queuecommand			= mptscsih_qcmd,
	.target_alloc			= mptspi_target_alloc,
	.slave_alloc			= mptspi_slave_alloc,
	.slave_configure		= mptspi_slave_configure,
	.target_destroy			= mptscsih_target_destroy,
	.slave_destroy			= mptspi_slave_destroy,
	.change_queue_depth 		= mptscsih_change_queue_depth,
	.eh_abort_handler		= mptscsih_abort,
	.eh_device_reset_handler	= mptscsih_dev_reset,
	.eh_bus_reset_handler		= mptscsih_bus_reset,
	.eh_host_reset_handler		= mptscsih_host_reset,
	.bios_param			= mptscsih_bios_param,
	.can_queue			= MPT_SCSI_CAN_QUEUE,
	.this_id			= -1,
	.sg_tablesize			= MPT_SCSI_SG_DEPTH,
	.max_sectors			= 8192,
	.cmd_per_lun			= 7,
	.use_clustering			= ENABLE_CLUSTERING,
};

static int mptspi_write_spi_device_pg1(struct scsi_target *starget,
			       struct _CONFIG_PAGE_SCSI_DEVICE_1 *pass_pg1)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)shost->hostdata;
	struct _MPT_ADAPTER *ioc = hd->ioc;
	struct _CONFIG_PAGE_SCSI_DEVICE_1 *pg1;
	dma_addr_t pg1_dma;
	int size;
	struct _x_config_parms cfg;
	struct _CONFIG_PAGE_HEADER hdr;
	int err = -EBUSY;

	/* don't allow updating nego parameters on RAID devices */
	if (starget->channel == 0 &&
	    (hd->ioc->raid_data.isRaid & (1 << starget->id)))
		return -1;

	size = ioc->spi_data.sdp1length * 4;

	pg1 = dma_alloc_coherent(&ioc->pcidev->dev, size, &pg1_dma, GFP_KERNEL);
	if (pg1 == NULL) {
		starget_printk(KERN_ERR, starget, "dma_alloc_coherent for parameters failed\n");
		return -EINVAL;
	}

	memset(&hdr, 0, sizeof(hdr));

	hdr.PageVersion = ioc->spi_data.sdp1version;
	hdr.PageLength = ioc->spi_data.sdp1length;
	hdr.PageNumber = 1;
	hdr.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

	memset(&cfg, 0, sizeof(cfg));

	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = pg1_dma;
	cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	cfg.dir = 1;
	cfg.pageAddr = starget->id;

	memcpy(pg1, pass_pg1, size);

	pg1->Header.PageVersion = hdr.PageVersion;
	pg1->Header.PageLength = hdr.PageLength;
	pg1->Header.PageNumber = hdr.PageNumber;
	pg1->Header.PageType = hdr.PageType;

	if (mpt_config(ioc, &cfg)) {
		starget_printk(KERN_ERR, starget, "mpt_config failed\n");
		goto out_free;
	}
	err = 0;

 out_free:
	dma_free_coherent(&ioc->pcidev->dev, size, pg1, pg1_dma);
	return err;
}

static void mptspi_write_offset(struct scsi_target *starget, int offset)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	u32 nego;

	if (offset < 0)
		offset = 0;

	if (offset > 255)
		offset = 255;

	if (spi_offset(starget) == -1)
		mptspi_read_parameters(starget);

	spi_offset(starget) = offset;

	nego = mptspi_getRP(starget);

	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

static void mptspi_write_period(struct scsi_target *starget, int period)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	u32 nego;

	if (period < 8)
		period = 8;

	if (period > 255)
		period = 255;

	if (spi_period(starget) == -1)
		mptspi_read_parameters(starget);

	if (period == 8) {
		spi_iu(starget) = 1;
		spi_dt(starget) = 1;
	} else if (period == 9) {
		spi_dt(starget) = 1;
	}

	spi_period(starget) = period;

	nego = mptspi_getRP(starget);

	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

static void mptspi_write_dt(struct scsi_target *starget, int dt)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	u32 nego;

	if (spi_period(starget) == -1)
		mptspi_read_parameters(starget);

	if (!dt && spi_period(starget) < 10)
		spi_period(starget) = 10;

	spi_dt(starget) = dt;

	nego = mptspi_getRP(starget);


	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

static void mptspi_write_iu(struct scsi_target *starget, int iu)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	u32 nego;

	if (spi_period(starget) == -1)
		mptspi_read_parameters(starget);

	if (!iu && spi_period(starget) < 9)
		spi_period(starget) = 9;

	spi_iu(starget) = iu;

	nego = mptspi_getRP(starget);

	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

#define MPTSPI_SIMPLE_TRANSPORT_PARM(parm) 				\
static void mptspi_write_##parm(struct scsi_target *starget, int parm)\
{									\
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;				\
	u32 nego;							\
									\
	spi_##parm(starget) = parm;					\
									\
	nego = mptspi_getRP(starget);					\
									\
	pg1.RequestedParameters = cpu_to_le32(nego);			\
	pg1.Reserved = 0;						\
	pg1.Configuration = 0;						\
									\
	mptspi_write_spi_device_pg1(starget, &pg1);				\
}

MPTSPI_SIMPLE_TRANSPORT_PARM(rd_strm)
MPTSPI_SIMPLE_TRANSPORT_PARM(wr_flow)
MPTSPI_SIMPLE_TRANSPORT_PARM(rti)
MPTSPI_SIMPLE_TRANSPORT_PARM(hold_mcs)
MPTSPI_SIMPLE_TRANSPORT_PARM(pcomp_en)

static void mptspi_write_qas(struct scsi_target *starget, int qas)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)shost->hostdata;
	VirtTarget *vtarget = starget->hostdata;
	u32 nego;

	if ((vtarget->negoFlags & MPT_TARGET_NO_NEGO_QAS) ||
	    hd->ioc->spi_data.noQas)
		spi_qas(starget) = 0;
	else
		spi_qas(starget) = qas;

	nego = mptspi_getRP(starget);

	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

static void mptspi_write_width(struct scsi_target *starget, int width)
{
	struct _CONFIG_PAGE_SCSI_DEVICE_1 pg1;
	u32 nego;

	if (!width) {
		spi_dt(starget) = 0;
		if (spi_period(starget) < 10)
			spi_period(starget) = 10;
	}

	spi_width(starget) = width;

	nego = mptspi_getRP(starget);

	pg1.RequestedParameters = cpu_to_le32(nego);
	pg1.Reserved = 0;
	pg1.Configuration = 0;

	mptspi_write_spi_device_pg1(starget, &pg1);
}

struct work_queue_wrapper {
	struct work_struct	work;
	struct _MPT_SCSI_HOST	*hd;
	int			disk;
};

static void mpt_work_wrapper(void *data)
{
	struct work_queue_wrapper *wqw = (struct work_queue_wrapper *)data;
	struct _MPT_SCSI_HOST *hd = wqw->hd;
	struct Scsi_Host *shost = hd->ioc->sh;
	struct scsi_device *sdev;
	int disk = wqw->disk;
	struct _CONFIG_PAGE_IOC_3 *pg3;

	kfree(wqw);

	mpt_findImVolumes(hd->ioc);
	pg3 = hd->ioc->raid_data.pIocPg3;
	if (!pg3)
		return;

	shost_for_each_device(sdev,shost) {
		struct scsi_target *starget = scsi_target(sdev);
		VirtTarget *vtarget = starget->hostdata;

		/* only want to search RAID components */
		if (sdev->channel != 1)
			continue;

		/* The target_id is the raid PhysDiskNum, even if
		 * starget->id is the actual target address */
		if(vtarget->target_id != disk)
			continue;

		starget_printk(KERN_INFO, vtarget->starget,
			       "Integrated RAID requests DV of new device\n");
		mptspi_dv_device(hd, sdev);
	}
	shost_printk(KERN_INFO, shost,
		     "Integrated RAID detects new device %d\n", disk);
	scsi_scan_target(&hd->ioc->sh->shost_gendev, 1, disk, 0, 1);
}


static void mpt_dv_raid(struct _MPT_SCSI_HOST *hd, int disk)
{
	struct work_queue_wrapper *wqw = kmalloc(sizeof(*wqw), GFP_ATOMIC);

	if (!wqw) {
		shost_printk(KERN_ERR, hd->ioc->sh,
			     "Failed to act on RAID event for physical disk %d\n",
			   disk);
		return;
	}
	INIT_WORK(&wqw->work, mpt_work_wrapper, wqw);
	wqw->hd = hd;
	wqw->disk = disk;

	schedule_work(&wqw->work);
}

static int
mptspi_event_process(MPT_ADAPTER *ioc, EventNotificationReply_t *pEvReply)
{
	u8 event = le32_to_cpu(pEvReply->Event) & 0xFF;
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)ioc->sh->hostdata;

	if (hd && event ==  MPI_EVENT_INTEGRATED_RAID) {
		int reason
			= (le32_to_cpu(pEvReply->Data[0]) & 0x00FF0000) >> 16;

		if (reason == MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED) {
			int disk = (le32_to_cpu(pEvReply->Data[0]) & 0xFF000000) >> 24;
			mpt_dv_raid(hd, disk);
		}
	}
	return mptscsih_event_process(ioc, pEvReply);
}

static int
mptspi_deny_binding(struct scsi_target *starget)
{
	struct _MPT_SCSI_HOST *hd =
		(struct _MPT_SCSI_HOST *)dev_to_shost(starget->dev.parent)->hostdata;
	return ((hd->ioc->raid_data.isRaid & (1 << starget->id)) &&
		starget->channel == 0) ? 1 : 0;
}

static struct spi_function_template mptspi_transport_functions = {
	.get_offset	= mptspi_read_parameters,
	.set_offset	= mptspi_write_offset,
	.show_offset	= 1,
	.get_period	= mptspi_read_parameters,
	.set_period	= mptspi_write_period,
	.show_period	= 1,
	.get_width	= mptspi_read_parameters,
	.set_width	= mptspi_write_width,
	.show_width	= 1,
	.get_iu		= mptspi_read_parameters,
	.set_iu		= mptspi_write_iu,
	.show_iu	= 1,
	.get_dt		= mptspi_read_parameters,
	.set_dt		= mptspi_write_dt,
	.show_dt	= 1,
	.get_qas	= mptspi_read_parameters,
	.set_qas	= mptspi_write_qas,
	.show_qas	= 1,
	.get_wr_flow	= mptspi_read_parameters,
	.set_wr_flow	= mptspi_write_wr_flow,
	.show_wr_flow	= 1,
	.get_rd_strm	= mptspi_read_parameters,
	.set_rd_strm	= mptspi_write_rd_strm,
	.show_rd_strm	= 1,
	.get_rti	= mptspi_read_parameters,
	.set_rti	= mptspi_write_rti,
	.show_rti	= 1,
	.get_pcomp_en	= mptspi_read_parameters,
	.set_pcomp_en	= mptspi_write_pcomp_en,
	.show_pcomp_en	= 1,
	.get_hold_mcs	= mptspi_read_parameters,
	.set_hold_mcs	= mptspi_write_hold_mcs,
	.show_hold_mcs	= 1,
	.deny_binding	= mptspi_deny_binding,
};

/****************************************************************************
 * Supported hardware
 */

static struct pci_device_id mptspi_pci_table[] = {
	{ PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_53C1030,
		PCI_ANY_ID, PCI_ANY_ID },
	{ PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_1030_53C1035,
		PCI_ANY_ID, PCI_ANY_ID },
	{0}	/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, mptspi_pci_table);


/*
 * renegotiate for a given target
 */
static void
mptspi_dv_renegotiate_work(void *data)
{
	struct work_queue_wrapper *wqw = (struct work_queue_wrapper *)data;
	struct _MPT_SCSI_HOST *hd = wqw->hd;
	struct scsi_device *sdev;

	kfree(wqw);

	shost_for_each_device(sdev, hd->ioc->sh)
		mptspi_dv_device(hd, sdev);
}

static void
mptspi_dv_renegotiate(struct _MPT_SCSI_HOST *hd)
{
	struct work_queue_wrapper *wqw = kmalloc(sizeof(*wqw), GFP_ATOMIC);

	if (!wqw)
		return;

	INIT_WORK(&wqw->work, mptspi_dv_renegotiate_work, wqw);
	wqw->hd = hd;

	schedule_work(&wqw->work);
}

/*
 * spi module reset handler
 */
static int
mptspi_ioc_reset(MPT_ADAPTER *ioc, int reset_phase)
{
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)ioc->sh->hostdata;
	int rc;

	rc = mptscsih_ioc_reset(ioc, reset_phase);

	if (reset_phase == MPT_IOC_POST_RESET)
		mptspi_dv_renegotiate(hd);

	return rc;
}

#ifdef CONFIG_PM
/*
 * spi module resume handler
 */
static int
mptspi_resume(struct pci_dev *pdev)
{
	MPT_ADAPTER 	*ioc = pci_get_drvdata(pdev);
	struct _MPT_SCSI_HOST *hd = (struct _MPT_SCSI_HOST *)ioc->sh->hostdata;
	int rc;

	rc = mptscsih_resume(pdev);
	mptspi_dv_renegotiate(hd);

	return rc;
}
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptspi_probe - Installs scsi devices per bus.
 *	@pdev: Pointer to pci_dev structure
 *
 *	Returns 0 for success, non-zero for failure.
 *
 */
static int
mptspi_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct Scsi_Host	*sh;
	MPT_SCSI_HOST		*hd;
	MPT_ADAPTER 		*ioc;
	unsigned long		 flags;
	int			 ii;
	int			 numSGE = 0;
	int			 scale;
	int			 ioc_cap;
	int			error=0;
	int			r;

	if ((r = mpt_attach(pdev,id)) != 0)
		return r;

	ioc = pci_get_drvdata(pdev);
	ioc->DoneCtx = mptspiDoneCtx;
	ioc->TaskCtx = mptspiTaskCtx;
	ioc->InternalCtx = mptspiInternalCtx;

	/*  Added sanity check on readiness of the MPT adapter.
	 */
	if (ioc->last_state != MPI_IOC_STATE_OPERATIONAL) {
		printk(MYIOC_s_WARN_FMT
		  "Skipping because it's not operational!\n",
		  ioc->name);
		error = -ENODEV;
		goto out_mptspi_probe;
	}

	if (!ioc->active) {
		printk(MYIOC_s_WARN_FMT "Skipping because it's disabled!\n",
		  ioc->name);
		error = -ENODEV;
		goto out_mptspi_probe;
	}

	/*  Sanity check - ensure at least 1 port is INITIATOR capable
	 */
	ioc_cap = 0;
	for (ii=0; ii < ioc->facts.NumberOfPorts; ii++) {
		if (ioc->pfacts[ii].ProtocolFlags &
		    MPI_PORTFACTS_PROTOCOL_INITIATOR)
			ioc_cap ++;
	}

	if (!ioc_cap) {
		printk(MYIOC_s_WARN_FMT
			"Skipping ioc=%p because SCSI Initiator mode is NOT enabled!\n",
			ioc->name, ioc);
		return 0;
	}

	sh = scsi_host_alloc(&mptspi_driver_template, sizeof(MPT_SCSI_HOST));

	if (!sh) {
		printk(MYIOC_s_WARN_FMT
			"Unable to register controller with SCSI subsystem\n",
			ioc->name);
		error = -1;
		goto out_mptspi_probe;
        }

	spin_lock_irqsave(&ioc->FreeQlock, flags);

	/* Attach the SCSI Host to the IOC structure
	 */
	ioc->sh = sh;

	sh->io_port = 0;
	sh->n_io_port = 0;
	sh->irq = 0;

	/* set 16 byte cdb's */
	sh->max_cmd_len = 16;

	/* Yikes!  This is important!
	 * Otherwise, by default, linux
	 * only scans target IDs 0-7!
	 * pfactsN->MaxDevices unreliable
	 * (not supported in early
	 *	versions of the FW).
	 * max_id = 1 + actual max id,
	 * max_lun = 1 + actual last lun,
	 *	see hosts.h :o(
	 */
	sh->max_id = MPT_MAX_SCSI_DEVICES;

	sh->max_lun = MPT_LAST_LUN + 1;
	/*
	 * If RAID Firmware Detected, setup virtual channel
	 */
	if ((ioc->facts.ProductID & MPI_FW_HEADER_PID_PROD_MASK)
	    > MPI_FW_HEADER_PID_PROD_TARGET_SCSI)
		sh->max_channel = 1;
	else
		sh->max_channel = 0;
	sh->this_id = ioc->pfacts[0].PortSCSIID;

	/* Required entry.
	 */
	sh->unique_id = ioc->id;

	/* Verify that we won't exceed the maximum
	 * number of chain buffers
	 * We can optimize:  ZZ = req_sz/sizeof(SGE)
	 * For 32bit SGE's:
	 *  numSGE = 1 + (ZZ-1)*(maxChain -1) + ZZ
	 *               + (req_sz - 64)/sizeof(SGE)
	 * A slightly different algorithm is required for
	 * 64bit SGEs.
	 */
	scale = ioc->req_sz/(sizeof(dma_addr_t) + sizeof(u32));
	if (sizeof(dma_addr_t) == sizeof(u64)) {
		numSGE = (scale - 1) *
		  (ioc->facts.MaxChainDepth-1) + scale +
		  (ioc->req_sz - 60) / (sizeof(dma_addr_t) +
		  sizeof(u32));
	} else {
		numSGE = 1 + (scale - 1) *
		  (ioc->facts.MaxChainDepth-1) + scale +
		  (ioc->req_sz - 64) / (sizeof(dma_addr_t) +
		  sizeof(u32));
	}

	if (numSGE < sh->sg_tablesize) {
		/* Reset this value */
		dprintk((MYIOC_s_INFO_FMT
		  "Resetting sg_tablesize to %d from %d\n",
		  ioc->name, numSGE, sh->sg_tablesize));
		sh->sg_tablesize = numSGE;
	}

	spin_unlock_irqrestore(&ioc->FreeQlock, flags);

	hd = (MPT_SCSI_HOST *) sh->hostdata;
	hd->ioc = ioc;

	/* SCSI needs scsi_cmnd lookup table!
	 * (with size equal to req_depth*PtrSz!)
	 */
	hd->ScsiLookup = kcalloc(ioc->req_depth, sizeof(void *), GFP_ATOMIC);
	if (!hd->ScsiLookup) {
		error = -ENOMEM;
		goto out_mptspi_probe;
	}

	dprintk((MYIOC_s_INFO_FMT "ScsiLookup @ %p\n",
		 ioc->name, hd->ScsiLookup));

	/* Allocate memory for the device structures.
	 * A non-Null pointer at an offset
	 * indicates a device exists.
	 * max_id = 1 + maximum id (hosts.h)
	 */
	hd->Targets = kcalloc(sh->max_id * (sh->max_channel + 1),
			      sizeof(void *), GFP_ATOMIC);
	if (!hd->Targets) {
		error = -ENOMEM;
		goto out_mptspi_probe;
	}

	dprintk((KERN_INFO "  vdev @ %p\n", hd->Targets));

	/* Clear the TM flags
	 */
	hd->tmPending = 0;
	hd->tmState = TM_STATE_NONE;
	hd->resetPending = 0;
	hd->abortSCpnt = NULL;

	/* Clear the pointer used to store
	 * single-threaded commands, i.e., those
	 * issued during a bus scan, dv and
	 * configuration pages.
	 */
	hd->cmdPtr = NULL;

	/* Initialize this SCSI Hosts' timers
	 * To use, set the timer expires field
	 * and add_timer
	 */
	init_timer(&hd->timer);
	hd->timer.data = (unsigned long) hd;
	hd->timer.function = mptscsih_timer_expired;

	ioc->spi_data.Saf_Te = mpt_saf_te;
	hd->mpt_pq_filter = mpt_pq_filter;

	hd->negoNvram = MPT_SCSICFG_USE_NVRAM;
	ddvprintk((MYIOC_s_INFO_FMT
		"saf_te %x mpt_pq_filter %x\n",
		ioc->name,
		mpt_saf_te,
		mpt_pq_filter));
	ioc->spi_data.noQas = 0;

	init_waitqueue_head(&hd->scandv_waitq);
	hd->scandv_wait_done = 0;
	hd->last_queue_full = 0;

	/* Some versions of the firmware don't support page 0; without
	 * that we can't get the parameters */
	if (hd->ioc->spi_data.sdp0length != 0)
		sh->transportt = mptspi_transport_template;

	error = scsi_add_host (sh, &ioc->pcidev->dev);
	if(error) {
		dprintk((KERN_ERR MYNAM
		  "scsi_add_host failed\n"));
		goto out_mptspi_probe;
	}

	/*
	 * issue internal bus reset
	 */
	if (ioc->spi_data.bus_reset)
		mptscsih_TMHandler(hd,
		    MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
		    0, 0, 0, 0, 5);

	scsi_scan_host(sh);
	return 0;

out_mptspi_probe:

	mptscsih_remove(pdev);
	return error;
}

static struct pci_driver mptspi_driver = {
	.name		= "mptspi",
	.id_table	= mptspi_pci_table,
	.probe		= mptspi_probe,
	.remove		= __devexit_p(mptscsih_remove),
	.shutdown	= mptscsih_shutdown,
#ifdef CONFIG_PM
	.suspend	= mptscsih_suspend,
	.resume		= mptspi_resume,
#endif
};

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptspi_init - Register MPT adapter(s) as SCSI host(s) with
 *	linux scsi mid-layer.
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int __init
mptspi_init(void)
{
	show_mptmod_ver(my_NAME, my_VERSION);

	mptspi_transport_template = spi_attach_transport(&mptspi_transport_functions);
	if (!mptspi_transport_template)
		return -ENODEV;

	mptspiDoneCtx = mpt_register(mptscsih_io_done, MPTSPI_DRIVER);
	mptspiTaskCtx = mpt_register(mptscsih_taskmgmt_complete, MPTSPI_DRIVER);
	mptspiInternalCtx = mpt_register(mptscsih_scandv_complete, MPTSPI_DRIVER);

	if (mpt_event_register(mptspiDoneCtx, mptspi_event_process) == 0) {
		devtverboseprintk((KERN_INFO MYNAM
		  ": Registered for IOC event notifications\n"));
	}

	if (mpt_reset_register(mptspiDoneCtx, mptspi_ioc_reset) == 0) {
		dprintk((KERN_INFO MYNAM
		  ": Registered for IOC reset notifications\n"));
	}

	return pci_register_driver(&mptspi_driver);
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptspi_exit - Unregisters MPT adapter(s)
 *
 */
static void __exit
mptspi_exit(void)
{
	pci_unregister_driver(&mptspi_driver);

	mpt_reset_deregister(mptspiDoneCtx);
	dprintk((KERN_INFO MYNAM
	  ": Deregistered for IOC reset notifications\n"));

	mpt_event_deregister(mptspiDoneCtx);
	dprintk((KERN_INFO MYNAM
	  ": Deregistered for IOC event notifications\n"));

	mpt_deregister(mptspiInternalCtx);
	mpt_deregister(mptspiTaskCtx);
	mpt_deregister(mptspiDoneCtx);
	spi_release_transport(mptspi_transport_template);
}

module_init(mptspi_init);
module_exit(mptspi_exit);
