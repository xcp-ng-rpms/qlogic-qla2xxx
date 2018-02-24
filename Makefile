#
#Copyright (c) 2018-2023 Marvell.
#
OPTIONS := -mindirect-branch-register

sles_distro := $(wildcard /etc/SuSE-release)
rhel_distro := $(wildcard /etc/redhat-release)

# Check  to see if we should use thunk-extern for SLES
ifeq ($(sles_distro),)
SLES_VERSION = $(shell cat /etc/SuSE-release | grep VERSION | grep -o -P [0-9]+)
SLES_PATCHLEVEL = $(shell cat /etc/SuSE-release | grep PATCHLEVEL | grep -o -P [0-9]+)
PADDED_PATCHLEVEL = $(shell if [ 10 -gt $(SLES_PATCHLEVEL) ]; then echo 0$(SLES_PATCHLEVEL); else echo $(SLES_PATCHLEVEL); fi)
SLES_DISTRO_VER = "0x$(SLES_VERSION)$(PADDED_PATCHLEVEL)"
SUSE_VER = $(shell cat /etc/os-release  | grep VERSION= | cut -d= -f 2)
endif

SUSE_BRAND = $(shell cat /etc/SUSE-brand 2>/dev/null | grep VERSION | sed 's/VERSION = //')
SUSE_PATCHLEVEL = $(shell cat /etc/SuSE-release 2>/dev/null | grep PATCHLEVEL | sed 's/PATCHLEVEL = //')

ifeq ($(SUSE_BRAND), 12)
  ifneq ($(shell test $(SUSE_PATCHLEVEL) -gt 3 && echo thunk_extern),)
    USE_THUNK_EXTERN = 1
  endif
endif

ifeq ($(SUSE_BRAND), 15)
  USE_THUNK_EXTERN = 1
endif

ifneq ($(rhel_distro),)
RHEL_MAJVER := $(shell grep "RHEL_MAJOR" /usr/include/linux/version.h | sed -e 's/.*MAJOR \([0-9]\)/\1/')
RHEL_MINVER := $(shell grep "RHEL_MINOR" /usr/include/linux/version.h | sed -e 's/.*MINOR \([0-9]\)/\1/')
RHEL_DISTRO_VER = 0x$(RHEL_MAJVER)$(RHEL_MINVER)
endif

ifeq ($(RHEL_DISTRO_VER), 0x0610)
  USE_THUNK_EXTERN = 1
endif

ifeq ($(RHEL_MAJVER), 7)
  ifneq ($(shell test $(RHEL_MINVER) -gt 4 && echo thunk_extern),)
    $(warning OS version is $(RHEL_MAJVER).$(RHEL_MINVER))
    USE_THUNK_EXTERN = 1
  endif
endif

ifneq ($(RHEL_MAJVER),)
  RHEL_VER_VCHK = $(shell test $(RHEL_MAJVER) -ge 8 && echo 1)
  ifeq ($(RHEL_VER_VCHK), 1)
    USE_THUNK_EXTERN = 1
  endif
endif

ifeq ($(USE_THUNK_EXTERN),1)
    OPTIONS += -mindirect-branch=thunk-extern
else
    OPTIONS += -mindirect-branch=thunk-inline
endif

# ae
ifeq ($(SLES),)
    SLES := $(shell grep -so "SLES" /etc/os-release)
endif

CITRIX := $(shell grep -so "xenenterprise" /etc/redhat-release)
UBUNTU := $(shell lsb_release -is 2> /dev/null | grep Ubuntu)

KVER := $(shell uname -r)
INC_DIR := /lib/modules/$(KVER)/build/include
ifneq ($(SLES),)
	_KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
	INC_DIR := /usr/src/linux-$(_KVER)/include
endif

# Disable target builds
CONFIG_TCM_QLA2XXX := m
# Check if the base OS is SLES, and latest upstream kernel
# is cloned and installed
ifeq ($(wildcard $(INC_DIR)/scsi/scsi.h),)
	ifeq ($(SLES),)
		KVER := $(shell uname -r)
		INC_DIR := /lib/modules/$(KVER)/build/include
	endif
	ifeq ($(UBUNTU),)
		INC_DIR := /lib/modules/$(KVER)/source/include
	endif
endif

ifneq ($(debug),)
$(warning INC_DIR=$(INC_DIR))
endif

#
# set-def:-
# $(call set-def,<define>,<include-file>,<regex-pattern>)
# 	- returns <define> if pattern is in include file.
#
# pattern should have word boundaries (-w option) and should not have
# embedded space (use \s instead).
#
define set-def
$(shell grep -qsw "$(strip $3)" \
        $(INC_DIR)/$(strip $2) && echo "$(strip $1)")
endef

#
# set-def-ext:-
# $(call set-def-ext,<define>,<include-file>,<command>)
# 	- returns <define> if pattern is in <command> output
#
# pattern should have word boundaries (-w option) and should not have
# embedded space (use \s instead).
#
# Command invocation is something like:
# 	$ <command> <include-file> |grep <regex-pattern>
#
define set-def-ext
$(shell $4 $(INC_DIR)/$(strip $2) |grep -qsw "$(strip $3)" \
         && echo "$(strip $1)")
endef

DEFINES += $(call set-def,SCSI_CHANGE_QDEPTH,\
		scsi/scsi_device.h,scsi_change_queue_depth)
DEFINES += $(call set-def,SCSI_CHANGE_QTYPE,scsi/scsi_host.h,change_queue_type)
DEFINES += $(call set-def,SCSI_USE_CLUSTERING,scsi/scsi_host.h,use_clustering)
DEFINES += $(call set-def,SCSI_MAP_QUEUES,scsi/scsi_host.h,map_queues)
DEFINES += $(call set-def,SCSI_MARGINAL_PATH_SUPPORT,scsi/scsi_host.h,\
		eh_should_retry_cmd)
DEFINES += $(call set-def,SCSI_CHANGE_QDEPTH_3ARGS,\
		scsi/scsi_host.h,change_queue_depth.*int.\sint)
DEFINES += $(call set-def,SCSI_HOST_WIDE_TAGS,\
		scsi/scsi_host.h,use_host_wide_tags)
DEFINES += $(call set-def,SCSI_USE_BLK_MQ,scsi/scsi_host.h,shost_use_blk_mq)
DEFINES += $(call set-def,SCSI_HAS_TCQ,scsi/scsi_tcq.h,scsi_activate_tcq)
DEFINES += $(call set-def,SCSI_CMD_TAG_ATTR,\
		scsi/scsi_tcq.h,scsi_populate_tag_msg)
DEFINES += $(call set-def,SCSI_FC_BSG_JOB,scsi/scsi_transport_fc.h,fc_bsg_job)
DEFINES += $(call set-def,REFCOUNT_READ,linux/refcount.h,refcount_read)
DEFINES += $(call set-def,TIMER_SETUP,linux/timer.h,define\stimer_setup)
DEFINES += $(call set-def,DMA_ZALLOC_COHERENT,\
		linux/dma-mapping.h,dma_zalloc_coherent)
DEFINES += $(call set-def,KTIME_GET_REAL_SECONDS,\
		linux/timekeeping.h,ktime_get_real_seconds)
DEFINES += $(call set-def,NVME_POLL_QUEUE,linux/nvme-fc-driver.h,(.poll_queue))
DEFINES += $(call set-def,DEFINED_FPIN_RCV,scsi/scsi_transport_fc.h,\
	fc_host_fpin_rcv)
DEFINES += $(call set-def,SCSI_CMD_PRIV,scsi/sci_cmnd.h,scsi_cmd_priv)
DEFINES += $(call set-def,T10_PI_APP_ESC,linux/t10-pi.h,T10_PI_APP_ESCAPE)
DEFINES += $(call set-def,T10_PI_REF_ESC,linux/t10-pi.h,T10_PI_REF_ESCAPE)
DEFINES += $(call set-def,T10_PI_TUPLE,linux/t10-pi.h,t10_pi_tuple)
DEFINES += $(call set-def,FC_EH_TIMED_OUT,scsi/sci_transport_fc.h,fc_eh_timed_out)
DEFINES += $(call set-def,SCSI_TRACK_QUE_DEPTH,scsi/scsi_host.h,track_queue_depth)
DEFINES += $(call set-def,PCI_ERR_RESET_PREPARE,linux/pci.h,reset_prepare)
DEFINES += $(call set-def,PCI_ERR_RESET_DONE,linux/pci.h,reset_done)
DEFINES += $(call set-def,BLK_MQ_HCTX_TYPE,linux/blk-mq.h,hctx_type)
DEFINES += $(call set-def,SCSI_CHANGE_Q_DEPTH,scsi/scsi_device.h,scsi_change_queue_depth)
DEFINES += $(call set-def,FPIN_EVENT_TYPES,uapi/scsi/fc/fc_els.h,fc_fpin_deli_event_types)
DEFINES += $(call set-def,SET_DRIVER_BYTE,scsi/scsi_cmnd.h,set_driver_byte)
DEFINES += $(call set-def,BLK_MQ_HCTX_TYPE,linux/blk-mq.h,hctx_type)
DEFINES += $(call set-def,LIST_IS_FIRST,linux/list.h,list_is_first)

DEFINES += $(call set-def,FPIN_EVENT_TYPES,uapi/scsi/fc/fc_els.h,fc_fpin_deli_event_types)
DEFINES += $(call set-def-ext,BLK_PCI_MAPQ_3_ARGS,\
	   linux/blk-mq-pci.h,offset,grep -A1 blk_mq_pci_map_queues)
DEFINES += $(call set-def-ext,NVME_FC_PORT_TEMPLATE_HV_MODULE,\
	   linux/nvme-fc-driver.h,module,grep -A1 "nvme_fc_port_template {")
DEFINES += $(call set-def-ext, NVME_FC_QUEUE_MAPPING,\
	   linux/nvme-fc-driver.h, blk_mq_queue_map, grep -A1 map_queues)
DEFINES += $(call set-def,SCSI_CMND_PROT_FLAGS,scsi/scsi_cmnd.h,prot_flags)
DEFINES += $(call set-def,TRACE_EVENTS_QLA,trace/events/qla.h,qla_log_event)
DEFINES += $(call set-def-ext,SCSI_SCSI_CMND_H_SCSI_DONE,\
		scsi/scsi_cmnd.h,scsi_done,grep -A8 "sense_buffer")
DEFINES += $(call set-def,SCSI_SCSI_CMND_H_SCSI_PROT_INTERVAL,scsi/scsi_cmnd.h,scsi_prot_interval)
DEFINES += $(call set-def,SCSI_CMD_TO_RQ,scsi/scsi_cmnd.h,scsi_cmd_to_rq)
DEFINES += $(call set-def,FC_BLOCK_RPORT,scsi/scsi_transport_fc.h,fc_block_rport)
DEFINES += $(call set-def,SCSI_BUILD_SENSE,scsi/scsi_cmnd.h,scsi_build_sense)
DEFINES += $(call set-def,EH_SHOULD_RETRY_CMD,scsi/scsi_host.h,eh_should_retry_cmd)
DEFINES += $(call set-def,SHOST_GROUPS,scsi/scsi_host.h,shost_groups)
DEFINES += $(call set-def,SCSI_PROT_REF_TAG,scsi/scsi_cmnd.h,scsi_prot_ref_tag)
DEFINES += $(call set-def,PANIC_NOTIFIER_LIST,linux/kernel.h,panic_notifier_list)

DEFINES += $(call set-def-ext,NVME_FC_PORT_TEMPLATE_MAP_QUEUES,\
	   linux/nvme-fc-driver.h,blk_mq_queue_map,grep -A1 "map_queues")
DEFINES += $(call set-def,NVME_FC_RCV_LS_REQUEST,linux/nvme-fc-driver.h,nvmefc_ls_rsp)
DEFINES += $(call set-def-ext,SCSI_MAP_QUEUES_RET_VOID,\
		scsi/scsi_host.h, void, grep -A1 "map_queues")
DEFINES += $(call set-def-ext,FPIN_RCV_4ARGS,scsi/scsi_transport_fc.h,\
	fc_host_fpin_rcv, grep -B1 "event_acknowledge")
#
# set-def-ifndef:-
# $(call set-def,<define>,<include-file>,<regex-pattern>)
#       - returns <define> if pattern is "NOT" in include file.
#
define set-def-ifndef
$(shell grep -qsw "$(strip $3)" $(INC_DIR)/$(strip $2) && : || echo "$(strip $1)")
endef

DEFINES += $(call set-def-ifndef, BE_ARRAY, linux/byteorder/generic.h, be32_to_cpu_array)

# These names should be same as used in qla_deprecated_unmaint_list
RHEL8=0
RHEL9=0
SLES15SP5=0
SLES15SP6=0
OTHERS=1

ifneq ($(RHEL_DISTRO_VER),)
DEFINES += RHEL_DISTRO_VERSION=$(RHEL_DISTRO_VER)
endif

ifeq ($(RHEL_MAJVER), 8)
RHEL8=1
OTHERS=0
endif

ifeq ($(RHEL_MAJVER), 9)
RHEL9=1
OTHERS=0
endif

ifeq ($(SUSE_VER), "15-SP5")
SLES15SP5=1
OTHERS=0
endif

ifeq ($(SUSE_VER), "15-SP6")
SLES15SP6=1
OTHERS=0
endif

DEFINES += RHEL8=$(RHEL8) RHEL9=$(RHEL9) SLES15SP5=$(SLES15SP5) SLES15SP6=$(SLES15SP6) OTHERS=$(OTHERS)

override EXTRA_CFLAGS += $(addprefix -D,$(DEFINES))

# Addition defines via command line, call: make EXTRA_DEFINES=XYZ
override EXTRA_CFLAGS += $(addprefix -D,$(EXTRA_DEFINES))

#For analyzing performance of driver, uncomment below flag
override EXTRA_CFLAGS += -DQLA2XXX_LATENCY_MEASURE

# -I$(src) is for qla.h / trace point compile
override EXTRA_CFLAGS +=  -I$(src)

ifneq ($(debug),)
$(warning EXTRA_CFLAGS=($(EXTRA_CFLAGS)))
endif

ifneq ($(debug),)
$(warning DEFINES=($(DEFINES)))
endif

# ae
ifneq ($(shell echo 'int main(){}' | gcc -x c $(OPTIONS) - 2>/dev/null && echo thunk), )
$(warning compiling with $(OPTIONS))
ccflags-y += $(OPTIONS)
else
ccflags-y += -DRETPOLINE
endif

qla2xxx-y := qla_os.o qla_init.o qla_mbx.o qla_iocb.o qla_isr.o qla_gs.o \
		qla_dbg.o qla_sup.o qla_attr.o qla_mid.o qla_dfs.o qla_bsg.o \
		qla_nx.o qla_mr.o qla_nx2.o qla_tmpl.o qla_nvme.o \
		qla_edif.o qla_scm.o

tcm_qla2xxx-y := tcm_stub.o

obj-$(CONFIG_SCSI_QLA_FC) += qla2xxx.o
obj-$(CONFIG_TCM_QLA2XXX) += tcm_qla2xxx.o
