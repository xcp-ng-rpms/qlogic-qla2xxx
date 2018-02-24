/*
 * Cavium Fibre Channel HBA Driver
 * Copyright (c)  2003-2016 QLogic Corporation
 * Copyright (C)  2016-2017 Cavium Inc
 * Copyright (c)  2018-2023 Marvell.
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#ifndef __QLA_COMPAT_H
#define __QLA_COMPAT_H

#ifndef DEFINED_FPIN_RCV
#define fc_host_fpin_rcv(_a, _b, _c)
#endif /* DEFINED_FPIN_RCV */

#ifdef FPIN_RCV_4ARGS
#define ql_fc_host_fpin_rcv(_a, _b, _c, _d)  fc_host_fpin_rcv(_a, _b, _c, _d)
#else
#define ql_fc_host_fpin_rcv(_a, _b, _c, _d)  fc_host_fpin_rcv(_a, _b, _c)
#endif

#ifdef SCSI_CHANGE_QDEPTH
#define QLA_SCSI_QUEUE_DEPTH \
	.change_queue_depth	= scsi_change_queue_depth,
#else /* SCSI_CHANGE_QDEPTH */
#include <scsi/scsi_tcq.h>
#define QLA_SCSI_QUEUE_DEPTH \
	.change_queue_depth	= qla2x00_change_queue_depth,
static inline
void qla2x00_adjust_sdev_qdepth_up(struct scsi_device *sdev, int qdepth)
{
	fc_port_t *fcport = sdev->hostdata;
	struct scsi_qla_host *vha = fcport->vha;
	struct req_que *req = NULL;

	req = vha->req;
	if (!req)
		return;

	if (req->max_q_depth <= sdev->queue_depth || req->max_q_depth < qdepth)
		return;

	if (sdev->ordered_tags)
		scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, qdepth);
	else
		scsi_adjust_queue_depth(sdev, MSG_SIMPLE_TAG, qdepth);

	ql_dbg(ql_dbg_io, vha, 0x302a,
	    "Queue depth adjusted-up to %d for nexus=%ld:%d:%d.\n",
	    sdev->queue_depth, fcport->vha->host_no, sdev->id, sdev->lun);
}

static inline
void qla2x00_handle_queue_full(struct scsi_device *sdev, int qdepth)
{
	fc_port_t *fcport = (struct fc_port *) sdev->hostdata;

	if (!scsi_track_queue_full(sdev, qdepth))
		return;

	ql_dbg(ql_dbg_io, fcport->vha, 0x3029,
	    "Queue depth adjusted-down to %d for nexus=%ld:%d:%d.\n",
	    sdev->queue_depth, fcport->vha->host_no, sdev->id, sdev->lun);
}

static inline
int qla2x00_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	switch (reason) {
	case SCSI_QDEPTH_DEFAULT:
		scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
		break;
	case SCSI_QDEPTH_QFULL:
		qla2x00_handle_queue_full(sdev, qdepth);
		break;
	case SCSI_QDEPTH_RAMP_UP:
		qla2x00_adjust_sdev_qdepth_up(sdev, qdepth);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return sdev->queue_depth;
}
#endif /* SCSI_CHANGE_QDEPTH */

#ifdef SCSI_MARGINAL_PATH_SUPPORT
#define QLA_SCSI_MARGINAL_PATH \
	.eh_should_retry_cmd	= fc_eh_should_retry_cmd,
#else
	#define QLA_SCSI_MARGINAL_PATH
#endif /* SCSI_MARGINAL_PATH_SUPPORT */

#ifdef SCSI_CHANGE_QTYPE
#define QLA_SCSI_QUEUE_TYPE \
	.change_queue_type	= qla2x00_change_queue_type,
static inline int
qla2x00_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}

#else /* SCSI_CHANGE_QTYPE */
#define QLA_SCSI_QUEUE_TYPE
#endif /* SCSI_CHANGE_QTYPE */

#ifdef SCSI_MAP_QUEUES
#define QLA_SCSI_MAP_QUEUES \
	.map_queues             = qla2xxx_map_queues,
#include <linux/blk-mq-pci.h>

#if defined(SCSI_MAP_QUEUES_RET_VOID)
static inline void qla2xxx_map_queues(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = (scsi_qla_host_t *)shost->hostdata;
#else
static inline int qla2xxx_map_queues(struct Scsi_Host *shost)
{
	int rc = -EINVAL;
	scsi_qla_host_t *vha = (scsi_qla_host_t *)shost->hostdata;
#endif

#if defined(SCSI_MAP_QUEUES_RET_VOID)
	struct blk_mq_queue_map *qmap = &shost->tag_set.map[HCTX_TYPE_DEFAULT];

	if (!vha->hw->mqiobase)
		blk_mq_map_queues(qmap);
	else
		blk_mq_pci_map_queues(qmap,
				vha->hw->pdev, vha->irq_offset);

#elif defined(BLK_MQ_HCTX_TYPE)
	struct blk_mq_queue_map *qmap = &shost->tag_set.map[HCTX_TYPE_DEFAULT];

	if (!vha->hw->mqiobase)
		rc = blk_mq_map_queues(qmap);
	else
		rc = blk_mq_pci_map_queues(qmap,
				vha->hw->pdev, vha->irq_offset);
#else

	if (!vha->hw->mqiobase)
		rc = blk_mq_map_queues(&shost->tag_set);
	else
#ifdef BLK_PCI_MAPQ_3_ARGS
		rc = blk_mq_pci_map_queues(
				(struct blk_mq_tag_set *)&shost->tag_set,
				vha->hw->pdev, vha->irq_offset);
#else /* BLK_PCI_MAPQ_3_ARGS */
		rc = blk_mq_pci_map_queues(
				(struct blk_mq_tag_set *)&shost->tag_set,
				vha->hw->pdev);
#endif /* BLK_PCI_MAPQ_3_ARGS */
#endif

#if !defined(SCSI_MAP_QUEUES_RET_VOID)
	return rc;
#endif
}
#else /* SCSI_MAP_QUEUES */
#define QLA_SCSI_MAP_QUEUES
#endif /* SCSI_MAP_QUEUES */


#ifdef SCSI_HOST_WIDE_TAGS
#define QLA_SCSI_HOST_WIDE_TAGS \
	.use_host_wide_tags = 1,
#else /* SCSI_HOST_WIDE_TAGS */
#define QLA_SCSI_HOST_WIDE_TAGS
#endif /* SCSI_HOST_WIDE_TAGS */

#define lun_cast(_a) (long long)(_a)

#ifdef SCSI_HAS_TCQ
static inline
void qla_scsi_tcq_handler(struct scsi_device *sdev)
{
	scsi_qla_host_t *vha = shost_priv(sdev->host);
	struct req_que *req = vha->req;

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, req->max_q_depth);
	else
		scsi_deactivate_tcq(sdev, req->max_q_depth);
}
#else /* SCSI_HAS_TCQ */
#define qla_scsi_tcq_handler(_sdev)
#endif /* SCSI_HAS_TCQ */

#ifdef SCSI_CMD_TAG_ATTR
#include <scsi/scsi_tcq.h>
static inline
int qla_scsi_get_task_attr(struct scsi_cmnd *cmd)
{
	char tag[2];
	if (scsi_populate_tag_msg(cmd, tag)) {
		switch (tag[0]) {
		case HEAD_OF_QUEUE_TAG:
		    return TSK_HEAD_OF_QUEUE;
		case ORDERED_QUEUE_TAG:
		    return TSK_ORDERED;
		default:
		    return TSK_SIMPLE;
		}
	}
	return TSK_SIMPLE;
}
#else /* SCSI_CMD_TAG_ATTR */
#define qla_scsi_get_task_attr(_cmd) (TSK_SIMPLE)
#endif /* SCSI_CMD_TAG_ATTR */

#ifdef SCSI_FC_BSG_JOB
#define fc_bsg_to_shost(_job) (_job)->shost
#define fc_bsg_to_rport(_job) (_job)->rport
#define bsg_job_done(_job, _res, _len) (_job)->job_done(_job)
#define qla_fwsts_ptr(_job) ((uint8_t *)(_job)->req->sense) + \
				sizeof(struct fc_bsg_reply)
#else /* SCSI_FC_BSG_JOB */
#define qla_fwsts_ptr(_job) ((_job)->reply + sizeof(struct fc_bsg_reply))
#endif /* SCSI_FC_BSG_JOB */

#ifdef TIMER_SETUP
#define qla_timer_setup(_tmr, _func, _flags, _cb) \
	timer_setup(_tmr, _func, _flags)
#define qla_from_timer(_var, _timer_arg, _field) \
	(typeof(*_var) *)from_timer(_var, _timer_arg, _field)
#else /* TIMER_SETUP */
#define qla_timer_setup(_tmr, _func, _flags, _cb) \
	init_timer(_tmr); \
	(_tmr)->data = (qla_timer_arg_t) (_cb); \
	(_tmr)->function = (void (*)(unsigned long))_func;
#define qla_from_timer(_var, _timer_arg, _field) \
	(typeof(*_var) *)(_timer_arg)
#endif /* TIMER_SETUP */

#ifdef DMA_ZALLOC_COHERENT
#else /* DMA_ZALLOC_COHERENT */
/* This version of dma_alloc_coherent() does zero out memory. */
#define dma_zalloc_coherent(_dev, _sz, _hdl, _flag) \
	dma_alloc_coherent(_dev, _sz, _hdl, _flag)
#endif /* DMA_ZALLOC_COHERENT */

#ifdef SCSI_USE_CLUSTERING
#define QLA_SCSI_USER_CLUSETERING\
	.use_clustering = ENABLE_CLUSTERING,
#else /* SCSI_USE_CLUSTERING */
#define QLA_SCSI_USER_CLUSETERING
#endif /* SCSI_USE_CLUSTERING */

#ifdef KTIME_GET_REAL_SECONDS
#define qla_get_real_seconds() ktime_get_real_seconds()
#else /* KTIME_GET_REAL_SECONDS */
static inline
u64 qla_get_real_seconds(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec;
}
#endif /* KTIME_GET_REAL_SECONDS */

#ifndef  FC_PORTSPEED_64GBIT
#define FC_PORTSPEED_64GBIT             0x1000
#endif
#ifndef  FC_PORTSPEED_128GBIT
#define FC_PORTSPEED_128GBIT            0x2000
#endif

#ifdef BE_ARRAY
static inline void cpu_to_be32_array(__be32 *dst, const u32 *src, size_t len)
{
	int i;

	for (i = 0; i < len; i++)
		dst[i] = cpu_to_be32(src[i]);
}

static inline void be32_to_cpu_array(u32 *dst, const __be32 *src, size_t len)
{
	int i;

	for (i = 0; i < len; i++)
		dst[i] = be32_to_cpu(src[i]);
}
#endif

#ifdef SCSI_USE_BLK_MQ
# ifdef RHEL_DISTRO_VERSION
# define rhel_set_blk_mq(_host) \
	(_host)->use_blk_mq = ql2xmqsupport ? true : false;
# else /* RHEL_DISTRO_VERSION */
# define rhel_set_blk_mq(_host)
# endif /* RHEL_DISTRO_VERSION */
#else /* SCSI_USE_BLK_MQ */
/* Legacy was killed off, return 1 always. */
#define shost_use_blk_mq(_host) 1
#define rhel_set_blk_mq(_host)
#endif /* SCSI_USE_BLK_MQ */

#ifdef NVME_POLL_QUEUE
static inline
void qla_nvme_poll(struct nvme_fc_local_port *lport, void *hw_queue_handle)
{
	struct qla_qpair *qpair = hw_queue_handle;
	unsigned long flags;
	struct scsi_qla_host *vha = lport->private;

	spin_lock_irqsave(qpair->qp_lock_ptr, flags);
	queue_work(vha->hw->wq, &qpair->q_work);
	spin_unlock_irqrestore(qpair->qp_lock_ptr, flags);
}
#define QLA_NVME_POLL_QUEUE \
	.poll_queue	= qla_nvme_poll,
#else /* NVME_POLL_QUEUE */
#define QLA_NVME_POLL_QUEUE
#endif /* NVME_POLL_QUEUE */

#define qla_scsi_templ_compat_entries \
	QLA_SCSI_QUEUE_DEPTH \
	QLA_SCSI_QUEUE_TYPE \
	QLA_SCSI_HOST_WIDE_TAGS \
	QLA_SCSI_USER_CLUSETERING \
	QLA_SCSI_MAP_QUEUES \
	QLA_SCSI_FC_EH_TIMED_OUT \
	QLA_SCSI_TRACK_QUE_DEPTH \
	QLA_SCSI_MARGINAL_PATH

#define qla_nvme_templ_compat_entries \
	QLA_NVME_POLL_QUEUE


#define qla_pci_err_handler_compat_entries \
	QLA_PCI_ERR_RESET_PREPARE \
	QLA_PCI_ERR_RESET_DONE

#ifdef SCSI_CMD_PRIV
typedef scsi_cmd_priv ql_scsi_cmd_priv;
#else /* SCSI_CMD_PRIV */
static inline void *ql_scsi_cmd_priv(struct scsi_cmnd *cmd)
{
	return cmd + 1;
}
#endif /*SCSI_CMD_PRIV */

#ifdef FC_EH_TIMED_OUT
#define QLA_SCSI_FC_EH_TIMED_OUT \
	.eh_timed_out = fc_eh_timed_out,
#else /* FC_EH_TIMED_OUT */
#define QLA_SCSI_FC_EH_TIMED_OUT
#endif /* FC_EH_TIMED_OUT */

#ifdef SCSI_TRACK_QUE_DEPTH
#define QLA_SCSI_TRACK_QUE_DEPTH \
	.track_queue_depth = 1,
#else /* SCSI_TRACK_QUE_DEPTH */
#define QLA_SCSI_TRACK_QUE_DEPTH
#endif /* SCSI_TRACK_QUE_DEPTH */

#ifdef PCI_ERR_RESET_PREPARE
#define QLA_PCI_ERR_RESET_PREPARE \
	.reset_prepare = qla_pci_reset_prepare,

static inline void
qla_pci_reset_prepare(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	struct qla_qpair *qpair;

	ql_log(ql_log_warn, base_vha, 0xffff,
	    "%s.\n", __func__);

	/*
	 * PCI FLR/function reset is about to reset the
	 * slot. Stop the chip to stop all DMA access.
	 * It is assumed that pci_reset_done will be called
	 * after FLR to resume Chip operation.
	 */
	ha->flags.eeh_busy = 1;
	mutex_lock(&ha->mq_lock);
	ha->base_qpair->online = 0;
	list_for_each_entry(qpair, &base_vha->qp_list, qp_list_elem)
		qpair->online = 0;
	mutex_unlock(&ha->mq_lock);

	set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
	qla2x00_abort_isp_cleanup(base_vha);
	qla2x00_abort_all_cmds(base_vha, DID_RESET << 16);
}
#else /* PCI_ERR_RESET_PREPARE */
#define QLA_PCI_ERR_RESET_PREPARE
#endif /* PCI_ERR_RESET_PREPARE */

#ifdef PCI_ERR_RESET_DONE
#define QLA_PCI_ERR_RESET_DONE\
	.reset_done = qla_pci_reset_done,
static inline void
qla_pci_reset_done(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	struct qla_qpair *qpair;

	ql_log(ql_log_warn, base_vha, 0xffff,
	    "%s.\n", __func__);

	/*
	 * FLR just completed by PCI layer. Resume adapter
	 */
	ha->flags.eeh_busy = 0;
	mutex_lock(&ha->mq_lock);
	ha->base_qpair->online = 1;
	list_for_each_entry(qpair, &base_vha->qp_list, qp_list_elem)
		qpair->online = 1;
	mutex_unlock(&ha->mq_lock);

	base_vha->flags.online = 1;
	ha->isp_ops->abort_isp(base_vha);
	clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
}
#else /* PCI_ERR_RESET_DONE */
#define QLA_PCI_ERR_RESET_DONE
#endif /* PCI_ERR_RESET_DONE */

#ifndef MIN_NICE
#define MIN_NICE -20
#endif

#ifdef T10_PI_APP_ESC
#include <linux/t10-pi.h>
#define QL_T10_PI_APP_ESCAPE T10_PI_APP_ESCAPE
#else /* T10_PI_APP_ESC */
#define QL_T10_PI_APP_ESCAPE 0xffff
#endif /* T10_PI_APP_ESC */

#ifdef T10_PI_REF_ESC
#include <linux/t10-pi.h>
#define QL_T10_PI_REF_ESCAPE T10_PI_REF_ESCAPE
#else /* T10_PI_REF_ESC */
#define QL_T10_PI_REF_ESCAPE 0xffffffff
#endif /* T10_PI_REF_ESC */

#ifdef T10_PI_TUPLE
#include <linux/t10-pi.h>
typedef struct t10_pi_tuple QL_T10_PI_TUPLE;
#else /* T10_PI_TUPLE */
/*
 * (sd.h is not exported, hence local inclusion)
 * Data Integrity Field tuple.
 */
struct sd_dif_tuple {
	__be16 guard_tag;	/* Checksum */
	__be16 app_tag;		/* Opaque storage */
	__be32 ref_tag;		/* Target LBA or indirect LBA */
};

typedef struct sd_dif_tuple QL_T10_PI_TUPLE;
#endif /* T10_PI_TUPLE */

#ifdef FPIN_EVENT_TYPES
#define DECLARE_ENUM2STR_LOOKUP_DELI_EVENT DECLARE_ENUM2STR_LOOKUP( \
		qla_get_dn_event_type, fc_fpin_deli_event_types, \
		QL_FPIN_DELI_EVT_TYPES_INIT);
#define DECLARE_ENUM2STR_LOOKUP_CONGN_EVENT DECLARE_ENUM2STR_LOOKUP( \
		qla_get_congn_event_type, fc_fpin_congn_event_types, \
		FC_FPIN_CONGN_EVT_TYPES_INIT);
#else
#define DECLARE_ENUM2STR_LOOKUP_DELI_EVENT DECLARE_ENUM2STR_LOOKUP( \
		qla_get_dn_event_type, ql_fpin_deli_event_types, \
		QL_FPIN_DELI_EVT_TYPES_INIT);
#define DECLARE_ENUM2STR_LOOKUP_CONGN_EVENT DECLARE_ENUM2STR_LOOKUP( \
		qla_get_congn_event_type, ql_fpin_congn_event_types, \
		QL_FPIN_CONGN_EVT_TYPES_INIT);
/*
 * Delivery event types
 */
enum ql_fpin_deli_event_types {
	FPIN_DELI_UNKNOWN =		0x0,
	FPIN_DELI_TIMEOUT =		0x1,
	FPIN_DELI_UNABLE_TO_ROUTE =	0x2,
	FPIN_DELI_DEVICE_SPEC =		0xF,
};

/*
 * Congestion event types
 */
enum ql_fpin_congn_event_types {
	FPIN_CONGN_CLEAR =		0x0,
	FPIN_CONGN_LOST_CREDIT =	0x1,
	FPIN_CONGN_CREDIT_STALL =	0x2,
	FPIN_CONGN_OVERSUBSCRIPTION =	0x3,
	FPIN_CONGN_DEVICE_SPEC =	0xF,
};

/*
 * Initializer useful for decoding table.
 * Please keep this in sync with the above definitions.
 */
#define QL_FPIN_CONGN_EVT_TYPES_INIT {				\
	{ FPIN_CONGN_CLEAR,		"Clear" },		\
	{ FPIN_CONGN_LOST_CREDIT,	"Lost Credit" },	\
	{ FPIN_CONGN_CREDIT_STALL,	"Credit Stall" },	\
	{ FPIN_CONGN_OVERSUBSCRIPTION,	"Oversubscription" },	\
	{ FPIN_CONGN_DEVICE_SPEC,	"Device Specific" },	\
}

#endif

#ifdef NVME_FC_PORT_TEMPLATE_HV_MODULE
#define NVME_FC_PORT_TEMPLATE_MODULE .module = THIS_MODULE,
#else
#define NVME_FC_PORT_TEMPLATE_MODULE
#endif

#ifndef fallthrough
#  if defined(__GNUC__) && __GNUC__ >= 7
#    define fallthrough   __attribute__((__fallthrough__))
#  else
#    define fallthrough   do {} while (0)  /* fallthrough */
#  endif
#endif



/* rhel 9.0 support */

#ifndef ioremap_nocache
#define ioremap_nocache ioremap
#endif


#ifndef SET_DRIVER_BYTE
#define DRIVER_SENSE	0x08
static inline void set_driver_byte(struct scsi_cmnd *cmd, char status)
{
	cmd->result = (cmd->result & 0x00ffffff) | (status << 24);
}
#endif

#ifdef SCSI_CMND_PROT_FLAGS
# define QLA_T10PI_IP_CHKSUM(cmd) (cmd->prot_flags & SCSI_PROT_IP_CHECKSUM)
# define QLA_DO_REF_TAG_CHECK(cmd) (cmd->prot_flags & SCSI_PROT_REF_CHECK)
# define QLA_T10PI_DISABLE_GUARD_CHECK(cmd) (!(cmd->prot_flags & SCSI_PROT_GUARD_CHECK))
#else
# define QLA_T10PI_IP_CHKSUM(cmd) (scsi_host_get_guard(cmd->device->host) & SHOST_DIX_GUARD_IP)
# define QLA_DO_REF_TAG_CHECK(cmd) (scsi_get_prot_type(cmd) == SCSI_PROT_DIF_TYPE0 || \
	scsi_get_prot_type(cmd) == SCSI_PROT_DIF_TYPE1 || \
	scsi_get_prot_type(cmd) == SCSI_PROT_DIF_TYPE2)
# define QLA_T10PI_DISABLE_GUARD_CHECK(cmd) (false)
#endif // SCSI_CMND_PROT_FLAGS

#ifdef SCSI_PROT_REF_TAG
# define QLA_T10PI_SET_REF_TAG(cmd, pkt) (pkt->ref_tag = cpu_to_le32(scsi_prot_ref_tag(cmd)))
#else
# define QLA_T10PI_SET_REF_TAG(cmd, pkt) \
	if (QLA_DO_REF_TAG_CHECK(cmd)) { \
	   pkt->ref_tag = cpu_to_le32((uint32_t)(0xffffffff & scsi_get_lba(cmd))); \
	}
#endif //SCSI_PROT_REF_TAG

/* Fabric Perf Impact Notification */
#define ELS_COMMAND_FPIN        0x16
#define ELS_COMMAND_RDP         0x18
/* Read Diagnostic Functions */
#define ELS_COMMAND_RDF         0x19
#define ELS_COMMAND_EDC         0x17
#define ELS_COMMAND_PUN         0x31

#if defined(SCSI_SCSI_CMND_H_SCSI_DONE)
# define CALL_SCSI_DONE(cmd)  cmd->scsi_done(cmd);
#else
# define CALL_SCSI_DONE(cmd) scsi_done(cmd);
#endif //SCSI_SCSI_CMND_H_SCSI_DONE

#ifndef SCSI_SCSI_CMND_H_SCSI_PROT_INTERVAL
#define scsi_prot_interval(cmd) cmd->device->sector_size
#endif

#ifndef SCSI_CMD_TO_RQ
#define scsi_cmd_to_rq(cmd) cmd->request
#endif

#ifdef FC_BLOCK_RPORT
#define qla_fc_block_rport(rport, cmd) fc_block_rport(rport)
#else
#define qla_fc_block_rport(rport, cmd) fc_block_scsi_eh(cmd)
#endif

#ifndef SCSI_BUILD_SENSE
#include  <scsi/scsi_eh.h>
static inline void scsi_build_sense(struct scsi_cmnd *scmd, int desc, u8 key, u8 asc, u8 ascq)
{
	scsi_build_sense_buffer(desc, scmd->sense_buffer, key, asc, ascq);
	scmd->result = SAM_STAT_CHECK_CONDITION;
}
#endif

#ifdef EH_SHOULD_RETRY_CMD
#define  QLA_EH_SHOULD_RETRY_CMD  .eh_should_retry_cmd = fc_eh_should_retry_cmd,
#else
#define QLA_EH_SHOULD_RETRY_CMD
#endif

#ifdef SHOST_GROUPS
#define QLA_QLA2X00_HOST_ATTRS \
struct attribute *qla2x00_host_attrs[] = {\
	&dev_attr_driver_version.attr,\
	&dev_attr_fw_version.attr,\
	&dev_attr_serial_num.attr,\
	&dev_attr_isp_name.attr,\
	&dev_attr_isp_id.attr,\
	&dev_attr_model_name.attr,\
	&dev_attr_model_desc.attr,\
	&dev_attr_pci_info.attr,\
	&dev_attr_link_state.attr,\
	&dev_attr_zio.attr,\
	&dev_attr_zio_timer.attr,\
	&dev_attr_beacon.attr,\
	&dev_attr_beacon_config.attr,\
	&dev_attr_optrom_bios_version.attr,\
	&dev_attr_optrom_efi_version.attr,\
	&dev_attr_optrom_fcode_version.attr,\
	&dev_attr_optrom_fw_version.attr,\
	&dev_attr_84xx_fw_version.attr,\
	&dev_attr_total_isp_aborts.attr,\
	&dev_attr_serdes_version.attr,\
	&dev_attr_mpi_version.attr,\
	&dev_attr_phy_version.attr,\
	&dev_attr_flash_block_size.attr,\
	&dev_attr_vlan_id.attr,\
	&dev_attr_vn_port_mac_address.attr,\
	&dev_attr_fabric_param.attr,\
	&dev_attr_fw_state.attr,\
	&dev_attr_optrom_gold_fw_version.attr,\
	&dev_attr_thermal_temp.attr,\
	&dev_attr_diag_requests.attr,\
	&dev_attr_diag_megabytes.attr,\
	&dev_attr_fw_dump_size.attr,\
	&dev_attr_allow_cna_fw_dump.attr,\
	&dev_attr_pep_version.attr,\
	&dev_attr_min_supported_speed.attr,\
	&dev_attr_max_supported_speed.attr,\
	&dev_attr_zio_threshold.attr,\
	&dev_attr_dif_bundle_statistics.attr,\
	&dev_attr_port_speed.attr,\
	&dev_attr_port_no.attr,\
	&dev_attr_fw_attr.attr,\
	&dev_attr_dport_diagnostics.attr,\
	&dev_attr_mpi_pause.attr,\
	&dev_attr_ql2xiniexchg.attr,\
	&dev_attr_nvme_connect_str.attr,\
	&dev_attr_uscm_stat.attr,\
	&dev_attr_uscm_profile.attr,\
	&dev_attr_uscm_vl.attr,\
	&dev_attr_mpi_fw_state.attr,\
	NULL,\
};\
static umode_t qla_host_attr_is_visible(struct kobject *kobj, struct attribute *attr, int i)\
{\
	return attr->mode;\
}\
static const struct attribute_group qla2x00_host_attr_group = {\
	.is_visible = qla_host_attr_is_visible,\
	.attrs = qla2x00_host_attrs\
};\
const struct attribute_group *qla2x00_host_groups[] = {\
	&qla2x00_host_attr_group,\
	NULL\
};

extern const struct attribute_group *qla2x00_host_groups[];
# define QLA_SHOST_GROUPS .shost_groups = qla2x00_host_groups,

#else

#define QLA_QLA2X00_HOST_ATTRS \
struct device_attribute *qla2x00_host_attrs[] = {\
	&dev_attr_driver_version,\
	&dev_attr_fw_version,\
	&dev_attr_serial_num,\
	&dev_attr_isp_name,\
	&dev_attr_isp_id,\
	&dev_attr_model_name,\
	&dev_attr_model_desc,\
	&dev_attr_pci_info,\
	&dev_attr_link_state,\
	&dev_attr_zio,\
	&dev_attr_zio_timer,\
	&dev_attr_beacon,\
	&dev_attr_beacon_config,\
	&dev_attr_optrom_bios_version,\
	&dev_attr_optrom_efi_version,\
	&dev_attr_optrom_fcode_version,\
	&dev_attr_optrom_fw_version,\
	&dev_attr_84xx_fw_version,\
	&dev_attr_total_isp_aborts,\
	&dev_attr_serdes_version,\
	&dev_attr_mpi_version,\
	&dev_attr_phy_version,\
	&dev_attr_flash_block_size,\
	&dev_attr_vlan_id,\
	&dev_attr_vn_port_mac_address,\
	&dev_attr_fabric_param,\
	&dev_attr_fw_state,\
	&dev_attr_optrom_gold_fw_version,\
	&dev_attr_thermal_temp,\
	&dev_attr_diag_requests,\
	&dev_attr_diag_megabytes,\
	&dev_attr_fw_dump_size,\
	&dev_attr_allow_cna_fw_dump,\
	&dev_attr_pep_version,\
	&dev_attr_min_supported_speed,\
	&dev_attr_max_supported_speed,\
	&dev_attr_zio_threshold,\
	&dev_attr_dif_bundle_statistics,\
	&dev_attr_port_speed,\
	&dev_attr_port_no,\
	&dev_attr_fw_attr,\
	&dev_attr_dport_diagnostics,\
	&dev_attr_mpi_pause,\
	&dev_attr_ql2xiniexchg,\
	&dev_attr_nvme_connect_str,\
	&dev_attr_uscm_stat,\
	&dev_attr_uscm_profile,\
	&dev_attr_uscm_vl,\
	&dev_attr_mpi_fw_state,\
	NULL,\
};\

extern struct device_attribute *qla2x00_host_attrs[];
#define QLA_SHOST_GROUPS .shost_attrs = qla2x00_host_attrs,
#endif


#ifndef DEFINE_SHOW_ATTRIBUTE
#define DEFINE_SHOW_ATTRIBUTE(__name)                                   \
static int __name ## _open(struct inode *inode, struct file *file)      \
{                                                                       \
	return single_open(file, __name ## _show, inode->i_private);    \
}                                                                       \
									\
static const struct file_operations __name ## _fops = {                 \
	.owner          = THIS_MODULE,                                  \
	.open           = __name ## _open,                              \
	.read           = seq_read,                                     \
	.llseek         = seq_lseek,                                    \
	.release        = single_release,                               \
}
#endif

#ifndef LIST_IS_FIRST
/**
 * list_is_first -- tests whether @ list is the first entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_first(const struct list_head *list,
	const struct list_head *head)
{
	return list->prev == head;
}
#endif

#if defined(SCSI_MAP_QUEUES_RET_VOID)
# define NVME_FC_TEMPLATE_MAP_QUEUES  .map_queues     = qla_nvme_map_queues,
#define QLA_NVME_MAP_QUEUES \
static void qla_nvme_map_queues(struct nvme_fc_local_port *lport, struct blk_mq_queue_map *map)\
{\
	struct scsi_qla_host *vha = lport->private;\
	blk_mq_pci_map_queues(map, vha->hw->pdev, vha->irq_offset);\
}

#elif defined(NVME_FC_PORT_TEMPLATE_MAP_QUEUES)
# define NVME_FC_TEMPLATE_MAP_QUEUES  .map_queues     = qla_nvme_map_queues,
#define QLA_NVME_MAP_QUEUES \
static void qla_nvme_map_queues(struct nvme_fc_local_port *lport, struct blk_mq_queue_map *map)\
{\
	struct scsi_qla_host *vha = lport->private;\
	int rc;\
\
	rc = blk_mq_pci_map_queues(map, vha->hw->pdev, vha->irq_offset);\
	if (rc)\
		ql_log(ql_log_warn, vha, 0x21de, "pci map queue failed 0x%x", rc);\
}

#else
# define NVME_FC_TEMPLATE_MAP_QUEUES
# define QLA_NVME_MAP_QUEUES
#endif

#ifndef NVME_FC_RCV_LS_REQUEST
#define NVME_FC_XMIT_LS_RESPONSE
struct nvmefc_ls_rsp {
	void *rspbuf;
	dma_addr_t rspdma;
	u16 rsplen;
	void (*done)(struct nvmefc_ls_rsp *rsp);
	void *nvme_fc_private;
};
#else
#define NVME_FC_XMIT_LS_RESPONSE .xmt_ls_rsp = qla_nvme_xmt_ls_rsp,
#endif

static inline
int qla_nvme_fc_rcv_ls_req(struct nvme_fc_remote_port *portptr,
			   struct nvmefc_ls_rsp *lsrsp, void *lsreqbuf,
			   u32 lsreqbuf_len)
{
#ifndef NVME_FC_RCV_LS_REQUEST
	ql_log(ql_log_warn, NULL, 0x2111,
	       "Kernel version doesn't support nvme_fc_rcv_ls_req() API.\n");
	return -EINVAL;
#else
	return nvme_fc_rcv_ls_req(portptr, lsrsp, lsreqbuf, lsreqbuf_len);
#endif
}

#endif /* __QLA_COMPAT_H */
