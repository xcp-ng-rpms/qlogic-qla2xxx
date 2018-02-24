/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2014 QLogic Corporation
 * Copyright (c)  2018-2023 Marvell.
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#ifndef __QLA_BSG_H
#define __QLA_BSG_H

/* BSG Vendor specific commands */
#define QL_VND_LOOPBACK		0x01
#define QL_VND_A84_RESET	0x02
#define QL_VND_A84_UPDATE_FW	0x03
#define QL_VND_A84_MGMT_CMD	0x04
#define QL_VND_IIDMA		0x05
#define QL_VND_FCP_PRIO_CFG_CMD	0x06
#define QL_VND_READ_FLASH	0x07
#define QL_VND_UPDATE_FLASH	0x08
#define QL_VND_SET_FRU_VERSION	0x0B
#define QL_VND_READ_FRU_STATUS	0x0C
#define QL_VND_WRITE_FRU_STATUS	0x0D
#define QL_VND_DIAG_IO_CMD	0x0A
#define QL_VND_WRITE_I2C	0x10
#define QL_VND_READ_I2C		0x11
#define QL_VND_FX00_MGMT_CMD	0x12
#define QL_VND_SERDES_OP	0x13
#define	QL_VND_SERDES_OP_EX	0x14
#define QL_VND_GET_FLASH_UPDATE_CAPS    0x15
#define QL_VND_SET_FLASH_UPDATE_CAPS    0x16
#define QL_VND_GET_BBCR_DATA    0x17
#define QL_VND_GET_PRIV_STATS	0x18
#define QL_VND_DPORT_DIAGNOSTICS	0x19
#define QL_VND_GET_PRIV_STATS_EX	0x1A
#define QL_VND_SS_GET_FLASH_IMAGE_STATUS	0x1E

#define QL_VND_EDIF_MGMT                0X1F
#define QL_VND_GET_PORT_SCM		0x20
#define QL_VND_GET_TARGET_SCM		0x21
#define QL_VND_GET_DRV_ATTR		0x22
#define QL_VND_MANAGE_HOST_STATS	0x23
#define QL_VND_GET_HOST_STATS		0x24
#define QL_VND_GET_TGT_STATS		0x25
#define QL_VND_MANAGE_HOST_PORT		0x26
#define QL_VND_SYSTEM_LOCKDOWN_INFO	0x27
#define QL_VND_BIDI_SCM_MGMT		0x28
#define QL_VND_GET_PORT_SCM_V2		0x29
#define QL_VND_GET_TARGET_SCM_V2	0x2A
#define QL_VND_MBX_PASSTHRU		0x2B
#define QL_VND_DPORT_DIAGNOSTICS_V2	0x2C
#define QL_VND_GET_RPORT_INFO	0x2D
#define QL_VND_SFP_FW_LOAD	0x2E
#define	QL_VND_READ_SFP	0x2F
#define	QL_VND_IMG_SET_VALID	0x30

/* BSG Vendor specific subcode returns */
#define EXT_STATUS_OK			0
#define EXT_STATUS_ERR			1
#define EXT_STATUS_BUSY			2
#define EXT_STATUS_INVALID_PARAM	6
#define EXT_STATUS_DATA_OVERRUN		7
#define EXT_STATUS_DATA_UNDERRUN	8
#define EXT_STATUS_MAILBOX		11
#define EXT_STATUS_BUFFER_TOO_SMALL	16
#define EXT_STATUS_NO_MEMORY		17
#define EXT_STATUS_DEVICE_OFFLINE	22

/*
 * To support bidirectional iocb
 * BSG Vendor specific returns
 */
#define EXT_STATUS_NOT_SUPPORTED	27
#define EXT_STATUS_INVALID_CFG		28
#define EXT_STATUS_DMA_ERR		29
#define EXT_STATUS_TIMEOUT		30
#define EXT_STATUS_THREAD_FAILED	31
#define EXT_STATUS_DATA_CMP_FAILED	32
#define EXT_STATUS_ADAPTER_IN_LOCKDOWN_MODE     39
#define EXT_STATUS_DPORT_DIAG_ERR	40
#define EXT_STATUS_DPORT_DIAG_IN_PROCESS	41
#define EXT_STATUS_DPORT_DIAG_NOT_RUNNING	42
#define EXT_STATUS_UNSUPPORTED_FW        43
#define EXT_STATUS_SFP_FW_UPDATE_IN_PROGRESS	44
#define EXT_STATUS_IMG_SET_VALID_ERR		47
#define EXT_STATUS_IMG_SET_CONFIG_ERR		48

/* BSG definations for interpreting CommandSent field */
#define INT_DEF_LB_LOOPBACK_CMD         0
#define INT_DEF_LB_ECHO_CMD             1

/* Loopback related definations */
#define INTERNAL_LOOPBACK		0xF1
#define EXTERNAL_LOOPBACK		0xF2
#define ENABLE_INTERNAL_LOOPBACK	0x02
#define ENABLE_EXTERNAL_LOOPBACK	0x04
#define INTERNAL_LOOPBACK_MASK		0x000E
#define MAX_ELS_FRAME_PAYLOAD		252
#define ELS_OPCODE_BYTE			0x10

/* BSG Vendor specific definations */
#define A84_ISSUE_WRITE_TYPE_CMD        0
#define A84_ISSUE_READ_TYPE_CMD         1
#define A84_CLEANUP_CMD                 2
#define A84_ISSUE_RESET_OP_FW           3
#define A84_ISSUE_RESET_DIAG_FW         4
#define A84_ISSUE_UPDATE_OPFW_CMD       5
#define A84_ISSUE_UPDATE_DIAGFW_CMD     6

/* SFP FW update sub commands */
#define QL_VND_SC_SFP_FW_UPDATE		0x1
#define QL_VND_SC_SFP_FW_RESULT		0x2

struct qla84_mgmt_param {
	union {
		struct {
			uint32_t start_addr;
		} mem; /* for QLA84_MGMT_READ/WRITE_MEM */
		struct {
			uint32_t id;
#define QLA84_MGMT_CONFIG_ID_UIF        1
#define QLA84_MGMT_CONFIG_ID_FCOE_COS   2
#define QLA84_MGMT_CONFIG_ID_PAUSE      3
#define QLA84_MGMT_CONFIG_ID_TIMEOUTS   4

		uint32_t param0;
		uint32_t param1;
	} config; /* for QLA84_MGMT_CHNG_CONFIG */

	struct {
		uint32_t type;
#define QLA84_MGMT_INFO_CONFIG_LOG_DATA         1 /* Get Config Log Data */
#define QLA84_MGMT_INFO_LOG_DATA                2 /* Get Log Data */
#define QLA84_MGMT_INFO_PORT_STAT               3 /* Get Port Statistics */
#define QLA84_MGMT_INFO_LIF_STAT                4 /* Get LIF Statistics  */
#define QLA84_MGMT_INFO_ASIC_STAT               5 /* Get ASIC Statistics */
#define QLA84_MGMT_INFO_CONFIG_PARAMS           6 /* Get Config Parameters */
#define QLA84_MGMT_INFO_PANIC_LOG               7 /* Get Panic Log */

		uint32_t context;
/*
* context definitions for QLA84_MGMT_INFO_CONFIG_LOG_DATA
*/
#define IC_LOG_DATA_LOG_ID_DEBUG_LOG                    0
#define IC_LOG_DATA_LOG_ID_LEARN_LOG                    1
#define IC_LOG_DATA_LOG_ID_FC_ACL_INGRESS_LOG           2
#define IC_LOG_DATA_LOG_ID_FC_ACL_EGRESS_LOG            3
#define IC_LOG_DATA_LOG_ID_ETHERNET_ACL_INGRESS_LOG     4
#define IC_LOG_DATA_LOG_ID_ETHERNET_ACL_EGRESS_LOG      5
#define IC_LOG_DATA_LOG_ID_MESSAGE_TRANSMIT_LOG         6
#define IC_LOG_DATA_LOG_ID_MESSAGE_RECEIVE_LOG          7
#define IC_LOG_DATA_LOG_ID_LINK_EVENT_LOG               8
#define IC_LOG_DATA_LOG_ID_DCX_LOG                      9

/*
* context definitions for QLA84_MGMT_INFO_PORT_STAT
*/
#define IC_PORT_STATISTICS_PORT_NUMBER_ETHERNET_PORT0   0
#define IC_PORT_STATISTICS_PORT_NUMBER_ETHERNET_PORT1   1
#define IC_PORT_STATISTICS_PORT_NUMBER_NSL_PORT0        2
#define IC_PORT_STATISTICS_PORT_NUMBER_NSL_PORT1        3
#define IC_PORT_STATISTICS_PORT_NUMBER_FC_PORT0         4
#define IC_PORT_STATISTICS_PORT_NUMBER_FC_PORT1         5


/*
* context definitions for QLA84_MGMT_INFO_LIF_STAT
*/
#define IC_LIF_STATISTICS_LIF_NUMBER_ETHERNET_PORT0     0
#define IC_LIF_STATISTICS_LIF_NUMBER_ETHERNET_PORT1     1
#define IC_LIF_STATISTICS_LIF_NUMBER_FC_PORT0           2
#define IC_LIF_STATISTICS_LIF_NUMBER_FC_PORT1           3
#define IC_LIF_STATISTICS_LIF_NUMBER_CPU                6

		} info; /* for QLA84_MGMT_GET_INFO */
	} u;
};

struct qla84_msg_mgmt {
	uint16_t cmd;
#define QLA84_MGMT_READ_MEM     0x00
#define QLA84_MGMT_WRITE_MEM    0x01
#define QLA84_MGMT_CHNG_CONFIG  0x02
#define QLA84_MGMT_GET_INFO     0x03
	uint16_t rsrvd;
	struct qla84_mgmt_param mgmtp;/* parameters for cmd */
	uint32_t len; /* bytes in payload following this struct */
	uint8_t payload[]; /* payload for cmd */
};

struct qla_sfp_data
{
	uint32_t options;
#define QLA_READ_SFP_CACHED_DATA	0x00
#define QLA_READ_SFP_CACHED_DATA_SKIP_CACHE	0x01
#define QLA_READ_SFP_COMPL_DATA	0x02
#define QLA_READ_SFP_COMPL_DATA_SKIP_CACHE	0x03

	uint32_t reserved;
	uint8_t a0[256];
	uint8_t a2[256];

} __attribute__((packed));

struct qla_bsg_a84_mgmt {
	struct qla84_msg_mgmt mgmt;
} __attribute__ ((packed));

struct qla_scsi_addr {
	uint16_t bus;
	uint16_t target;
} __attribute__ ((packed));

struct qla_ext_dest_addr {
	union {
		uint8_t wwnn[8];
		uint8_t wwpn[8];
		uint8_t id[4];
		struct qla_scsi_addr scsi_addr;
	} dest_addr;
	uint16_t dest_type;
#define	EXT_DEF_TYPE_WWPN	2
	uint16_t lun;
	uint16_t padding[2];
} __attribute__ ((packed));

struct qla_port_param {
	struct qla_ext_dest_addr fc_scsi_addr;
	uint16_t mode;
	uint16_t speed;
} __attribute__ ((packed));

struct qla_mbx_passthru {
	uint16_t reserved1[2];
	uint16_t mbx_in[32];
	uint16_t mbx_out[32];
	uint32_t reserved2[16];
} __packed;

/* FRU VPD */

#define MAX_FRU_SIZE	36

struct qla_field_address {
	uint16_t offset;
	uint16_t device;
	uint16_t option;
} __packed;

struct qla_field_info {
	uint8_t version[MAX_FRU_SIZE];
} __packed;

struct qla_image_version {
	struct qla_field_address field_address;
	struct qla_field_info field_info;
} __packed;

struct qla_image_version_list {
	uint32_t count;
	struct qla_image_version version[];
} __packed;

struct qla_status_reg {
	struct qla_field_address field_address;
	uint8_t status_reg;
	uint8_t reserved[7];
} __packed;

struct qla_i2c_access {
	uint16_t device;
	uint16_t offset;
	uint16_t option;
	uint16_t length;
	uint8_t  buffer[0x40];
} __packed;

/* 26xx serdes register interface */

/* serdes reg commands */
#define INT_SC_SERDES_READ_REG		1
#define INT_SC_SERDES_WRITE_REG		2

struct qla_serdes_reg {
	uint16_t cmd;
	uint16_t addr;
	uint16_t val;
} __packed;

struct qla_serdes_reg_ex {
	uint16_t cmd;
	uint32_t addr;
	uint32_t val;
} __packed;

struct qla_flash_update_caps {
	uint64_t  capabilities;
	uint32_t  outage_duration;
	uint8_t   reserved[20];
} __packed;

/* BB_CR Status */
#define QLA_BBCR_STATUS_DISABLED       0
#define QLA_BBCR_STATUS_ENABLED        1
#define QLA_BBCR_STATUS_UNKNOWN        2

/* BB_CR State */
#define QLA_BBCR_STATE_OFFLINE         0
#define QLA_BBCR_STATE_ONLINE          1

/* BB_CR Offline Reason Code */
#define QLA_BBCR_REASON_PORT_SPEED     1
#define QLA_BBCR_REASON_PEER_PORT      2
#define QLA_BBCR_REASON_SWITCH         3
#define QLA_BBCR_REASON_LOGIN_REJECT   4

struct  qla_bbcr_data {
	uint8_t   status;         /* 1 - enabled, 0 - Disabled */
	uint8_t   state;          /* 1 - online, 0 - offline */
	uint8_t   configured_bbscn;       /* 0-15 */
	uint8_t   negotiated_bbscn;       /* 0-15 */
	uint8_t   offline_reason_code;
	uint16_t  mbx1;			/* Port state */
	uint8_t   reserved[9];
} __packed;

struct qla_dport_diag {
	uint16_t options;
	uint32_t buf[16];
	uint8_t  unused[62];
} __packed;

#define QLA_GET_DPORT_RESULT_V2		0  /* Get Result */
#define QLA_RESTART_DPORT_TEST_V2	1  /* Restart test */
#define QLA_START_DPORT_TEST_V2		2  /* Start test */
struct qla_dport_diag_v2 {
	uint16_t options;
	uint16_t mbx1;
	uint16_t mbx2;
	uint8_t  unused[58];
	uint8_t buf[1024]; /* Test Result */
} __packed;

/* D_Port options */
#define QLA_DPORT_RESULT	0x0
#define QLA_DPORT_START		0x2

/* active images in flash */
struct qla_active_regions {
	uint8_t global_image;
	uint8_t board_config;
	uint8_t vpd_nvram;
	uint8_t npiv_config_0_1;
	uint8_t npiv_config_2_3;
	uint8_t nvme_params;
	uint8_t reserved[31];
} __packed;

#include "qla_edif_bsg.h"

enum ql_fpin_li_event_types {
	QL_FPIN_LI_UNKNOWN =		0x0,
	QL_FPIN_LI_LINK_FAILURE =	0x1,
	QL_FPIN_LI_LOSS_OF_SYNC =	0x2,
	QL_FPIN_LI_LOSS_OF_SIG =	0x3,
	QL_FPIN_LI_PRIM_SEQ_ERR =	0x4,
	QL_FPIN_LI_INVALID_TX_WD =	0x5,
	QL_FPIN_LI_INVALID_CRC =	0x6,
	QL_FPIN_LI_UNCORRECTABLE_FEC =	0x7,
	QL_FPIN_LI_DEVICE_SPEC =	0xF,
};

/*
 * Initializer useful for decoding table.
 * Please keep this in sync with the above definitions.
 */
#define QL_FPIN_LI_EVT_TYPES_INIT {					\
	{ QL_FPIN_LI_UNKNOWN,		"Unknown" },			\
	{ QL_FPIN_LI_LINK_FAILURE,	"Link Failure" },		\
	{ QL_FPIN_LI_LOSS_OF_SYNC,	"Loss of Synchronization" },	\
	{ QL_FPIN_LI_LOSS_OF_SIG,	"Loss of Signal" },		\
	{ QL_FPIN_LI_PRIM_SEQ_ERR,	"Primitive Sequence Protocol Error" }, \
	{ QL_FPIN_LI_INVALID_TX_WD,	"Invalid Transmission Word" },	\
	{ QL_FPIN_LI_INVALID_CRC,	"Invalid CRC" },		\
	{ QL_FPIN_LI_UNCORRECTABLE_FEC,	"Uncorrectable FEC Error" },		\
	{ QL_FPIN_LI_DEVICE_SPEC,	"Device Specific" },		\
}


#define SCM_LINK_EVENT_V1_SIZE			20
struct qla_scm_link_event {
	uint64_t	timestamp;
	uint16_t	event_type;
	uint16_t	event_modifier;
	uint32_t	event_threshold;
	uint32_t	event_count;
	uint8_t		reserved[12];
} __packed;

/*
 * Delivery event types
 */
#define QL_FPIN_DELI_EVT_TYPES_INIT {					\
	{ FPIN_DELI_UNKNOWN,		"Unknown" },			\
	{ FPIN_DELI_TIMEOUT,		"Timeout" },			\
	{ FPIN_DELI_UNABLE_TO_ROUTE,	"Unable to Route" },		\
	{ FPIN_DELI_DEVICE_SPEC,	"Device Specific" },		\
}

struct qla_scm_delivery_event {
	uint64_t	timestamp;
	uint32_t	delivery_reason;
	uint8_t		deliver_frame_hdr[24];
	uint8_t		reserved[28];

} __packed;

struct qla_scm_peer_congestion_event {
	uint64_t	timestamp;
	uint16_t	event_type;
	uint16_t	event_modifier;
	uint32_t	event_period;
	uint8_t		reserved[16];
} __packed;

#define SCM_CONGESTION_SEVERITY_WARNING	0xF1
#define SCM_CONGESTION_SEVERITY_ERROR	0xF7
struct qla_scm_congestion_event {
	uint64_t	timestamp;
	uint16_t	event_type;
	uint16_t	event_modifier;
	uint32_t	event_period;
	uint8_t		severity;
	uint8_t		reserved[15];
} __packed;

#define SCM_FLAG_RDF_REJECT		0x00
#define SCM_FLAG_RDF_COMPLETED		0x01
#define SCM_FLAG_BROCADE_CONNECTED	0x02
#define SCM_FLAG_CISCO_CONNECTED	0x04

enum ql_fpin_event_types {
	SCM_EVENT_NONE =		0x0,
	SCM_EVENT_CONGESTION =		0x1,
	SCM_EVENT_DELIVERY =		0x2,
	SCM_EVENT_LINK_INTEGRITY =	0x4,
	SCM_EVENT_PEER_CONGESTION =	0x8,
};

#define QL_FPIN_EVENT_TYPES_INIT {					\
	{ SCM_EVENT_NONE,		"None" },			\
	{ SCM_EVENT_CONGESTION,		"Congestion" },			\
	{ SCM_EVENT_DELIVERY,		"Delivery" },			\
	{ SCM_EVENT_LINK_INTEGRITY,	"Link Integrity" },		\
	{ SCM_EVENT_PEER_CONGESTION,	"Peer Congestion" },		\
}

#define SCM_STATE_HEALTHY		0x0
#define SCM_STATE_CONGESTED		0x1

#define QLA_CON_PRIMITIVE_RECEIVED	0x1
#define QLA_CONGESTION_ARB_WARNING	0x1
#define QLA_CONGESTION_ARB_ALARM	0x2

/* Virtual Lane Support */
#define QLA_VL_MODE_DISABLED 		0x0 /* Administratively disabled */
#define QLA_VL_MODE_OPERATIONAL		0x1 /* Negotiated with switch and operational */
#define QLA_VL_MODE_NON_OPERATIONAL	0x2 /* Administratively enabled, switch negotiation failed */

/* Virtual Lane States */
#define QLA_VL_STATE_DISABLED		0x0
#define QLA_VL_STATE_SLOW		0x1
#define QLA_VL_STATE_NORMAL		0x2
#define QLA_VL_STATE_FAST		0x3
/*
 * Fabric Performance Impact Notification Statistics
 */
struct qla_scm_stats {
	/* Delivery */
	u64 dn_unknown;
	u64 dn_timeout;
	u64 dn_unable_to_route;
	u64 dn_device_specific;

	/* Link Integrity */
	u64 li_failure_unknown;
	u64 li_link_failure_count;
	u64 li_loss_of_sync_count;
	u64 li_loss_of_signals_count;
	u64 li_prim_seq_err_count;
	u64 li_invalid_tx_word_count;
	u64 li_invalid_crc_count;
	u64 li_device_specific;

	/* Congestion/Peer Congestion */
	u64 cn_clear;
	u64 cn_lost_credit;
	u64 cn_credit_stall;
	u64 cn_oversubscription;
	u64 cn_device_specific;

	/* PUN Stats */
	u64 pun_count;
	u64 pun_clear_count;
} __packed;

struct qla_scmr_stats {
	uint64_t	throttle_cleared;
	uint64_t	throttle_down_count;
	uint64_t	throttle_up_count;
	uint64_t	busy_status_count;
	uint64_t	throttle_hit_low_wm;
} __packed;

struct qla_fpin_severity {
	uint64_t	cn_alarm;
	uint64_t	cn_warning;
} __packed;

enum ql_scm_profile_type {
	QL_SCM_MONITOR 		= 0,
	QL_SCM_CONSERVATIVE 	= 1,
	QL_SCM_MODERATE 	= 2,
	QL_SCM_AGGRESSIVE 	= 3
};

#define MAX_SCM_PROFILE 4

#define QL_SCM_PROFILE_TYPES_INIT {			\
	{ QL_SCM_MONITOR,	"Monitor" },		\
	{ QL_SCM_CONSERVATIVE,	"Conservative" },	\
	{ QL_SCM_MODERATE,	"Moderate" },		\
	{ QL_SCM_AGGRESSIVE,	"Aggressive" },		\
}

struct qla_scmr_port_profile {
#define QLA_USE_NVRAM_CONFIG		BIT(0)
#define QLA_USE_FW_SLOW_QUEUE		BIT(1)
#define QLA_APPLY_SCMR_THROTTLING	BIT(2)
	uint8_t scmr_control_flags;
	uint8_t scmr_profile;
	uint8_t rsvd[6];
} __packed;

struct qla_scm_host_config {
#define QLA_RESET_SCM_STATS		BIT(0)
#define QLA_RESET_SCMR_STATS		BIT(1)
#define QLA_APPLY_SCMR_PROFILE		BIT(2)
#define QLA_GET_SCMR_PROFILE		BIT(3)
	uint8_t		controls;
	struct qla_scmr_port_profile profile;
	uint8_t		reserved[15];
} __packed;

/* Driver's internal data structure */
struct qla_scm_port_combined {
	struct qla_scm_link_event	link_integrity;
	struct qla_scm_delivery_event	delivery;
	struct qla_scm_congestion_event	congestion;
	struct qla_scm_stats		stats;
	struct qla_fpin_severity	sev;
	struct qla_scmr_stats		rstats;

	uint32_t			last_event_timestamp;
	uint8_t			current_events;
#define QLA_DISP_MODE_COMPACT		0x0
#define QLA_DISP_MODE_DETAILED		0x1
	uint8_t				display_mode;
	uint8_t				scm_fabric_connection_flags;
	uint8_t				current_state;
	uint64_t			li_uncorrectable_fec_count;
} __packed;

struct qla_scm_port_v2 {
	struct qla_scm_stats		stats;
	struct qla_fpin_severity	sev;
	struct qla_scmr_stats		rstats;
	uint8_t				scm_fabric_connection_flags;
	uint8_t				current_state;
	uint32_t			secs_since_last_event;
	uint8_t				scm_events;
	uint8_t				vl_mode;
	uint8_t				io_throttling;
	uint64_t                        li_uncorrectable_fec_count;
	uint8_t				reserved[55];
} __packed;

struct qla_scm_port {
	uint32_t			current_events;

	struct qla_scm_link_event	link_integrity;
	struct qla_scm_delivery_event	delivery;
	struct qla_scm_congestion_event	congestion;
	uint64_t			scm_congestion_alarm;
	uint64_t			scm_congestion_warning;
	uint8_t				scm_fabric_connection_flags;
	uint8_t				reserved[43];
} __packed;

/* Driver's internal data structure */
struct qla_scm_target_combined {
	uint8_t				wwpn[8];

	struct qla_scm_link_event	link_integrity;
	struct qla_scm_delivery_event	delivery;
	struct qla_scm_peer_congestion_event	peer_congestion;

	struct qla_scm_stats		stats;
	struct qla_scmr_stats		rstats;
	uint32_t			last_event_timestamp;
	uint8_t				current_events;
	uint8_t				current_state;
	uint64_t			li_uncorrectable_fec_count;
};

struct qla_scm_target_v2 {
	uint8_t				wwpn[8];
	struct qla_scm_stats		stats;
	struct qla_scmr_stats		rstats;
	uint8_t				current_state;
	uint32_t			secs_since_last_event;
	uint8_t				scm_events;
	uint8_t				vl_state;
	uint8_t				io_throttling;
	uint64_t			li_uncorrectable_fec_count;
	uint8_t				reserved[56];
} __packed;

struct qla_scm_target {
	uint8_t		wwpn[8];
	uint32_t	current_events;

	struct qla_scm_link_event		link_integrity;
	struct qla_scm_delivery_event		delivery;
	struct qla_scm_peer_congestion_event	peer_congestion;

	uint32_t	link_failure_count;
	uint32_t	loss_of_sync_count;
	uint32_t        loss_of_signals_count;
	uint32_t        primitive_seq_protocol_err_count;
	uint32_t        invalid_transmission_word_count;
	uint32_t        invalid_crc_count;

	uint32_t        delivery_failure_unknown;
	uint32_t        delivery_timeout;
	uint32_t        delivery_unable_to_route;
	uint32_t        delivery_failure_device_specific;

	uint32_t        peer_congestion_clear;
	uint32_t        peer_congestion_lost_credit;
	uint32_t        peer_congestion_credit_stall;
	uint32_t        peer_congestion_oversubscription;
	uint32_t        peer_congestion_device_specific;
	uint32_t	link_unknown_event;
	uint32_t	link_device_specific_event;
	uint8_t		reserved[48];
} __packed;

#define QLA_DRV_ATTR_SCM_SUPPORTED		0x00800000
#define QLA_DRV_ATTR_LOCKDOWN_SUPPORT		0x02000000
#define QLA_DRV_ATTR_SCM_2_SUPPORTED		0x04000000	/* Bit 26 */
#define QLA_DRV_ATTR_SCM_UPSTREAM_SUPPORT	0x08000000	/* Bit 27 */
#define QLA_DRV_ATTR_SCMR_PROFILE_SUPPORT	0x10000000	/* Bit 28 */
#define QLA_DRV_ATTR_DPORT_V2_SUPPORT		0x20000000	/* Bit 29 */
#define QLA_DRV_ATTR_VIRTUAL_LANE_SUPPORT	0x40000000	/* Bit 30 */
#define QLA_DRV_ATTR_IO_THROTTLING_SUPPORT	0x80000000	/* Bit 31 */

struct qla_drv_attr {
	uint32_t	attributes;
	u32		ext_attributes;
#define QLA_RPORT_INFO_SUPPORT      BIT_0
#define QLA_DRV_EXT_ATTR_SFP_FW_LOAD_SUPPORT	BIT_1
#define QLA_DRV_MAINTENANCE_MODE_SUPPORT	BIT_2
#define QLA_CACHED_SFP_DATA_SUPPORT	BIT_3
#define QLA_IMG_SET_VALID_SUPPORT	BIT_4
	u32		status_flags;
#define QLA_STATUS_FLAG_DRV_MAINT_MODE	BIT_0
	uint8_t		reserved[20];
} __packed;

struct qla_mpi_lockdown_info {
	uint32_t  config_disable_flags;       //mbx3
	uint32_t  fw_update_disable_flags;    //mbx4
	uint32_t  mpi_disable_flags;          //mbx5
	uint32_t  lockdown_support;           //mbx2
} __attribute__ ((packed));

struct qla_lockdown_info {
	uint8_t   signature[4];
	struct qla_mpi_lockdown_info mpi_fw_lockdown;
	uint32_t   isp_fw_lockdown;
	uint8_t   reserved[40];
} __attribute__ ((packed));

struct qla_sfp_fw_load_info {
	uint16_t	mbx1;
				/* Outgoing mbx 1
				 * 0001h: SFP not present
				 * 0002h: FW cannot allocate the IOCB buffer
				 * 0074h: SFP firmware load failed
				 */
	uint16_t	mbx2;
				/* Outgoing mbx 2:
				 * Additional info when SFP FW load failed 74h)
				 * 0 – SFP failed to enter download mode
				 * 1 – SFP failed to start process
				 * 2 – SFP failed frame checksum
				 * 4 – SFP failed to exit download
				 * 5 – Selected SFP firmware does not match
				 *	the installed SFP
				 * 6 – SFP device download currently
				 *	not supported for this device
				 */
	uint16_t	mbx3;
				/* Original SFP FW version */
	uint16_t	mbx4;
				/* Updated SFP FW version when
				 * Outgoing MB 0 is 4000h
				 */
	uint8_t	reserved[24];
} __packed;


enum authentication_state {
	QLA_AUTH_NA,	   /* no secure target */
	QLA_AUTH_NEEDED,   /* notify strongswan to start negotiation */
	QLA_AUTH_PENDING,  /* negotiation in progress */
	QLA_AUTH_FAIL,     /* negotiation failed */
	QLA_AUTH_SUCCESS,  /* negotiation success */
	QLA_AUTH_TERMINATE,     /* negotiation terminating */
	QLA_AUTH_UNKNOWN,     /* negotiation in undesirable state */
};

struct qla_rport_info {
	port_id_t  nport_id;
	u32 initiator_mode:1;
	u32 target_mode:1;
	u32 type_nvme:1;
	u32 type_fcp:1;
	u32 secure_device:1;
	u32 online:1;

	u8  wwpn[WWN_SIZE];
	u8  wwnn[WWN_SIZE];
	u8  reserve0[8];

	/* The following fields are valid if secure_device = 1 */
	enum authentication_state auth_state;
	u32 auth_fail_cnt;
	u32 auth_success_cnt;

	u8 reserve2[20];  /* round off to 64 bytes */
} __packed;

struct qla_rport_info_reply {
	u16 port_count;  /* Number of ports return */
	u16 port_total;  /* This is the number of ports seen on switch */

	u8  version;
	u8  pad[3];
	u8  reserve[24];
	struct qla_rport_info ports[0];
} __packed;

#endif
