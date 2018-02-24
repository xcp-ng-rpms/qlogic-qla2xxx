/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2014 QLogic Corporation
 * Copyright (c)  2018-2022 Marvell.
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"
#include "qla_gbl.h"

/* SCM Private Functions */
static bool
qla2xxx_scmr_check_low_thruput(struct qla_scmr_flow_control *sfc)
{
	bool ret = false;

	if ((atomic64_read(&sfc->perf.bytes_last_sec)) <
		(atomic64_read(&sfc->base_bytes) * ql2x_scmr_drop_pct_low_wm)/100) {
		ret = true;
		sfc->rstats->throttle_hit_low_wm++;
		ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		    "USCM: Throughput drop from: %llu to: %llu (MB)\n",
			(long long unsigned int)
			atomic64_read(&sfc->base_bytes) >> 20,
			(long long unsigned int)
			atomic64_read(&sfc->perf.bytes_last_sec) >> 20);
	}

	return ret;
}

static bool
qla2xxx_scmr_check_low_wm(struct qla_scmr_flow_control *sfc,
			  int curr, int base)
{
	bool ret = false;
	uint8_t *port_name = qla_scmr_is_tgt(sfc) ? sfc->fcport->port_name: sfc->vha->port_name;

	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		if (qla_scmr_is_tgt(sfc)) {
			if (curr == QLA_MIN_TGT_Q_DEPTH)
				ret = true;
		} else {
			if (curr == QLA_MIN_HBA_Q_DEPTH)
				ret = true;
		}
	}

	if (ret == true) {
		ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		    "USCM: Reached low watermark, permitted: %d baseline: %d for:%8phN\n",
			curr, base, port_name);
		sfc->rstats->throttle_hit_low_wm++;
	} else {
		ret = qla2xxx_scmr_check_low_thruput(sfc);
	}
	return ret;
}

static void
qla2xxx_change_queue_depth(struct qla_scmr_flow_control *sfc, int new)
{
	if (qla_scmr_is_tgt(sfc)) {
		if (new >= QLA_MIN_TGT_Q_DEPTH)
			atomic_set(&sfc->scmr_permitted, new);
		else
			atomic_set(&sfc->scmr_permitted, QLA_MIN_TGT_Q_DEPTH);
	} else {
		if (new >= QLA_MIN_HBA_Q_DEPTH)
			atomic_set(&sfc->scmr_permitted, new);
		else
			atomic_set(&sfc->scmr_permitted, QLA_MIN_HBA_Q_DEPTH);
	}
}

bool
qla2xxx_switch_vl(struct qla_scmr_flow_control *sfc, uint8_t vl)
{
	if (sfc->vha->hw->flags.conn_fabric_cisco_er_rdy) {
		if (sfc->fcport->vl.v_lane != vl) {
			sfc->fcport->vl.v_lane = vl;
			qla_scmr_set_notify_fw(sfc);
			set_bit(SCM_NOTIFY_FW,
				&sfc->vha->dpc_flags);
			return true;
		}
	}
	return false;

}

DECLARE_ENUM2STR_LOOKUP(qla_get_profile_type, ql_scm_profile_type,
			QL_SCM_PROFILE_TYPES_INIT);

/* Reduce throttle based on IOs/period or bytes/period */
static void
qla2xxx_scmr_reduce_throttle(struct qla_scmr_flow_control *sfc)
{
	int current_val, new;
	bool low_wm = false;
	int qla_scmr_profile = sfc->profile.scmr_profile;
	uint8_t *port_name = qla_scmr_is_tgt(sfc) ? sfc->fcport->port_name: sfc->vha->port_name;

	current_val = new = 0;

	current_val = atomic_read(&sfc->scmr_permitted);
	if (current_val) {
		low_wm = qla2xxx_scmr_check_low_wm(sfc, current_val,
				atomic_read(&sfc->scmr_base));
	} else {
		current_val = atomic_read(&sfc->scmr_base);
	}

	if (low_wm == true) {
		ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		    "USCM: Hit low wm, no throttling \n");
		return;
	}

	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		if (sfc->dir == QLA_DIR_UP) {
			new = current_val - 1;
		} else {
			/* Profile defines the rate of throttling */
			new = current_val >> sfc->scmr_down_delta[qla_scmr_profile];
			sfc->down_delta = current_val - new;
		}
		qla2xxx_change_queue_depth(sfc, new);
	}

	sfc->rstats->throttle_down_count++;
	ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		"USCM: Congested, throttling down:%8phN, permitted: %d baseline: %d, profile %s\n",
		port_name, atomic_read(&sfc->scmr_permitted), atomic_read(&sfc->scmr_base),
		qla_get_profile_type(qla_scmr_profile));

	return;
}
/* Clear Baseline Throughput */
static void
qla2xxx_clear_baseline_tp(struct qla_scmr_flow_control *sfc)
{
	atomic64_set(&sfc->base_bytes, 0);
}

/* Set Baseline Throughput */
static void
qla2xxx_set_baseline_tp(struct qla_scmr_flow_control *sfc)
{
	atomic64_set(&sfc->base_bytes,
		atomic64_read(&sfc->perf.bytes_last_sec));
	ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		"USCM: Base Bytes %llu (MB)",
		(long long unsigned int)
		atomic64_read(&sfc->perf.bytes_last_sec) >> 20);
}


/* Increase by @ql2x_scmr_up_pct percent, every QLA_SCMR_THROTTLE_PERIOD
 * secs.
 */
static void
qla2xxx_scmr_increase_flows(struct qla_scmr_flow_control *sfc)
{
	int delta, current_val, base_val, new;
	int qla_scmr_profile = sfc->profile.scmr_profile;
	uint8_t *port_name = qla_scmr_is_tgt(sfc) ? sfc->fcport->port_name: sfc->vha->port_name;

	new = 0;

	if (sfc->throttle_period--)
		return;

	sfc->throttle_period = sfc->event_period + sfc->event_period_buffer;
	current_val = atomic_read(&sfc->scmr_permitted);
	base_val = atomic_read(&sfc->scmr_base);

	/* Unlikely */
	if (!current_val)
		return;

	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		if (qla_scmr_profile) {
			delta = sfc->scmr_up_delta[qla_scmr_profile];
			new = current_val + (sfc->down_delta > delta ?
				delta : sfc->down_delta - 1);

		}
	}

	if (new > base_val) {
		qla2xxx_scmr_clear_throttle(sfc);
		ql_log(ql_log_info, sfc->vha, 0x0203,
		    "USCM: Clearing throttle for:%8phN \n", port_name);
		/* Switch back to Normal */
		if (qla_scmr_is_tgt(sfc)) {
			qla2xxx_switch_vl(sfc, VL_NORMAL);
			qla2xxx_clear_baseline_tp(sfc);
		}
		return;
	} else {
		if (sfc->mode == QLA_MODE_Q_DEPTH) {
			qla2xxx_change_queue_depth(sfc, new);
		}
		sfc->dir = QLA_DIR_UP;
		sfc->rstats->throttle_up_count++;
		ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		    "USCM: throttling up, permitted: %d baseline: %d for:%8phN\n",
			atomic_read(&sfc->scmr_permitted), base_val, port_name);
	}
}

static void
qla2xxx_check_congestion_timeout(struct qla_scmr_flow_control *sfc)
{
	if (sfc->expiration_jiffies &&
	    (time_after(jiffies, sfc->expiration_jiffies))) {
		ql_log(ql_log_info, sfc->vha, 0x0203,
		    "USCM: Clearing Congestion, event period expired\n");
		qla2xxx_scmr_clear_congn(sfc);
		/* If there is no throttling, move to Normal lane */
		if ((sfc->dir == QLA_DIR_NONE) && qla_scmr_is_tgt(sfc)) {
			qla2xxx_switch_vl(sfc, VL_NORMAL);
			qla2xxx_clear_baseline_tp(sfc);
		}
	}

}

static bool
qla2xxx_check_fpin_event(struct qla_scmr_flow_control *sfc)
{
	if (qla_scmr_get_sig(sfc) == QLA_SIG_CLEAR) {
		qla_scmr_clear_sig(sfc, scmr_congn_signal);
		qla2xxx_scmr_clear_congn(sfc);
		ql_log(ql_log_info, sfc->vha, 0x0203,
		    "USCM:(H) Clear Congestion for WWN %8phN\n",
		    sfc->vha->port_name);
		if ((sfc->dir == QLA_DIR_NONE) /* There is no throttling */
			&& qla_scmr_is_tgt(sfc)) {
			qla2xxx_switch_vl(sfc, VL_NORMAL);
			qla2xxx_clear_baseline_tp(sfc);
		}
	}

	if (qla_scmr_get_sig(sfc) == QLA_SIG_CREDIT_STALL) {
		qla_scmr_clear_sig(sfc, scmr_congn_signal);
		return true;
	} else if (qla_scmr_get_sig(sfc) == QLA_SIG_OVERSUBSCRIPTION) {
		/* Check if profile policy asks for Global/Targeted Throttling (where relevant)
		 * Check if Targeted throttling is possible
		 */
			qla_scmr_clear_sig(sfc, scmr_congn_signal);
			return true;
	}
	return false;
}

static bool
qla2xxx_check_cn_event(struct qla_scmr_flow_control *sfc)
{
	bool congested = false;

	if (IS_ARB_CAPABLE(sfc->vha->hw)) {
		/* Handle ARB Signals */
		if (atomic_read(&sfc->num_sig_warning) >=
		    QLA_SCMR_WARN_THRESHOLD) {
			sfc->level = QLA_CONG_LOW;
			sfc->expiration_jiffies =
			    jiffies + (2 * HZ);
			congested = true;
			atomic_set(&sfc->num_sig_warning, 0);
		} else if (atomic_read(&sfc->num_sig_warning)) {
			ql_dbg(ql_dbg_scm, sfc->vha, 0xffff,
			    "USCM: Low congestion signals (warning): %d\n",
			    atomic_read(&sfc->num_sig_warning));
			atomic_set(&sfc->num_sig_warning, 0);
		}

		if (atomic_read(&sfc->num_sig_alarm) >=
		    QLA_SCMR_ALARM_THRESHOLD) {
			sfc->level = QLA_CONG_HIGH;
			sfc->expiration_jiffies =
			    jiffies + (2 * HZ);
			congested = true;
			atomic_set(&sfc->num_sig_alarm, 0);
		} else if (atomic_read(&sfc->num_sig_alarm)) {
			ql_dbg(ql_dbg_scm, sfc->vha, 0xffff,
			    "USCM: Low congestion signals (alarm) %d\n",
			    atomic_read(&sfc->num_sig_alarm));
			atomic_set(&sfc->num_sig_alarm, 0);
		}
	}

	if (congested == false)
		congested = qla2xxx_check_fpin_event(sfc);

	return congested;

}

#define SCMR_PERIODS_PER_SEC	10

static bool
qla2xxx_scmr_set_baseline(struct qla_scmr_flow_control *sfc)
{
	bool ret = false;

	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		if (atomic_read(&sfc->perf.max_q_depth) >
		    QLA_MIN_BASELINE_QDEPTH)
			atomic_set(&sfc->scmr_base,
			   atomic_read(&sfc->perf.max_q_depth));
		else
			atomic_set(&sfc->scmr_base,
			    QLA_MIN_BASELINE_QDEPTH);
		qla_scmr_set_throttle_qdepth(sfc);
		sfc->dir = QLA_DIR_DOWN;
		ret = true;
	}
	if ((ret == true) && (atomic64_read(&sfc->base_bytes) == 0))
		qla2xxx_set_baseline_tp(sfc);

	return ret;
}

static void
qla2xxx_reduce_flows(struct qla_scmr_flow_control *sfc)
{
	bool throttle = false;
	uint8_t *port_name = qla_scmr_is_tgt(sfc) ? sfc->fcport->port_name: sfc->vha->port_name;

	if (sfc->profile.scmr_profile == 0) {/* Monitor profile */
		ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
		    "USCM: Congested, No throttling (Monitor profile)\n");
		return;
	}

	/* Congestion Signal/FPIN received */
	if (!qla_scmr_reduced_throttle(sfc)) {
		throttle = qla2xxx_scmr_set_baseline(sfc);
	} else {
		throttle = true;
	}

	if (throttle == true)
		qla2xxx_scmr_reduce_throttle(sfc);
	else
		ql_log(ql_log_info, sfc->vha, 0x0203,
		    "USCM: IOs too low, not throttling for WWN %8phN\n",
		    port_name);

	if (!qla_scmr_is_congested(sfc)) {
		ql_log(ql_log_info, sfc->vha, 0x0203,
		    "USCM: Set Congestion for WWN %8phN\n", port_name);
		qla_scmr_set_congested(sfc);
		if (qla_scmr_is_tgt(sfc) &&
		    IS_NPVC_CAPABLE(sfc->vha->hw)) {
			qla_scmr_set_notify_fw(sfc);
			set_bit(SCM_NOTIFY_FW, &
				sfc->vha->dpc_flags);
		}
	}
}

static void
qla2xxx_handle_tgt_congestion(struct fc_port *fcport)
{
	bool congested, throttle;
	struct qla_scmr_flow_control *sfc = &fcport->sfc;

	congested = throttle = false;

	congested = qla2xxx_check_fpin_event(sfc);

	if (congested == true) {
		qla_scmr_set_congested(sfc);
		/* If a lane change is needed */
		if (qla2xxx_switch_vl(sfc, VL_SLOW)) {
			qla2xxx_set_baseline_tp(sfc);
			return;
		}
		qla2xxx_reduce_flows(sfc);
	} else {
		qla2xxx_check_congestion_timeout(sfc);
		if (!qla_scmr_reduced_throttle(sfc))
			return;

		qla2xxx_scmr_increase_flows(sfc);
	}
}

static void
qla2xxx_tune_host_throttle(struct qla_scmr_flow_control *sfc)
{
	bool congested = false;

	congested = qla2xxx_check_cn_event(sfc);
 
	if (congested == true) {
		qla2xxx_reduce_flows(sfc);
		qla_scmr_set_congested(sfc);
	} else {
		qla2xxx_check_congestion_timeout(sfc);
		if (!qla_scmr_reduced_throttle(sfc))
			return;

		qla2xxx_scmr_increase_flows(sfc);
	}
}

/*
 * qla2xxx_throttle_curr_req() - Check if this request should be sent
 * back for a retry because of congestion on this host.
 *
 * @sfc: Pointer to the flow control struct for the given request queue.
 * @cmd: SCSI Command.
 *
 * Returns true for retry, false otherwise.
 */
static bool
qla2xxx_throttle_curr_req(struct qla_scmr_flow_control *sfc)
{
	/* Throttle down reqs if the host has oversubscribed */

	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		if (qla_scmr_throttle_qdepth(sfc)) {
			if (atomic_read(&sfc->scmr_permitted) <
			    atomic_read(&sfc->perf.dir_q_depth)) {
				sfc->rstats->busy_status_count++;
				return true;
			}
		}
	}
	return false;
}

static inline void
qla2x00_restart_perf_timer(scsi_qla_host_t *vha)
{
	/* Currently used for 82XX only. */
	if (vha->device_flags & DFLG_DEV_FAILED) {
		ql_dbg(ql_dbg_scm, vha, 0x600d,
		    "Device in a failed state, returning.\n");
		return;
	}

	mod_timer(&vha->perf_timer, jiffies + HZ/10);
}

static void
qla2xxx_minute_stats(struct qla_scmr_flow_control *sfc)
{
	sfc->ticks++;

	if (!(sfc->ticks % 60)) {
		atomic_set(&sfc->perf.max_q_depth, 0);
	}
}

/* Externally Used APIs */

/**************************************************************************
*   qla2xxx_perf_timer
*
* Description:
*   100 ms timer. Should be maintained as a lightweight thread because
*   of its frequency.
*
* Context: Interrupt
***************************************************************************/
void
qla2xxx_perf_timer(qla_timer_arg_t t)
{
	scsi_qla_host_t *vha = qla_from_timer(vha, t, perf_timer);
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport;
	uint64_t index = 0;
        uint64_t count = 0;

	if (ha->flags.eeh_busy) {
		ql_dbg(ql_dbg_scm, vha, 0x6000,
		    "EEH = %d, restarting timer.\n",
		    ha->flags.eeh_busy);
		qla2x00_restart_perf_timer(vha);
		return;
	}

	index = ha->sfc.perf.index % 10;
	count = atomic64_read(&ha->sfc.perf.scmr_bytes_per_period);
	qla2xxx_atomic64_add(&ha->sfc.perf.bytes_last_sec, count);
	qla2xxx_atomic64_sub(&ha->sfc.perf.bytes_last_sec,
			ha->sfc.perf.bytes_arr[index]);
	ha->sfc.perf.bytes_arr[index] = count;

	atomic64_set(&ha->sfc.perf.scmr_bytes_per_period, 0);

	count = atomic_read(&ha->sfc.perf.scmr_reqs_per_period);
	qla2xxx_atomic_add(&ha->sfc.perf.reqs_last_sec, count);
	qla2xxx_atomic_sub(&ha->sfc.perf.reqs_last_sec,
					    ha->sfc.perf.reqs_arr[index]);
	ha->sfc.perf.reqs_arr[index] = count;
	atomic_set(&ha->sfc.perf.scmr_reqs_per_period, 0);
	ha->sfc.perf.index++;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
			continue;

		index = fcport->sfc.perf.index % 10;
		count = atomic64_read(&fcport->sfc.perf.scmr_bytes_per_period);
		qla2xxx_atomic64_add(&fcport->sfc.perf.bytes_last_sec, count);
		qla2xxx_atomic64_sub(&fcport->sfc.perf.bytes_last_sec,
				fcport->sfc.perf.bytes_arr[index]);
		fcport->sfc.perf.bytes_arr[index] = count;

		atomic64_set(&fcport->sfc.perf.scmr_bytes_per_period, 0);

		count = atomic_read(&fcport->sfc.perf.scmr_reqs_per_period);
		qla2xxx_atomic_add(&fcport->sfc.perf.reqs_last_sec, count);
		qla2xxx_atomic_sub(&fcport->sfc.perf.reqs_last_sec,
						    fcport->sfc.perf.reqs_arr[index]);
		fcport->sfc.perf.reqs_arr[index] = count;
		atomic_set(&fcport->sfc.perf.scmr_reqs_per_period, 0);
		fcport->sfc.perf.index++;
	}

	qla2x00_restart_perf_timer(vha);
}

/*
 * qla2xxx_throttle_req - To rate limit I/O on congestion.
 *
 * Returns true to throttle down, false otherwise.
 */
bool
qla2xxx_throttle_req(srb_t *sp, struct qla_hw_data *ha, fc_port_t *fcport, uint8_t dir)
{
	bool ret = false;

	/* NVMe enums map to the same values */
	if (dir == DMA_FROM_DEVICE &&
		qla_scm_chk_throttle_cmd_opcode(sp)) {
		ret = qla2xxx_throttle_curr_req(&ha->sfc);
		if (ret == true) {
			atomic_inc(&ha->throttle_read);
			return ret;
		}
	}

	if (dir == DMA_TO_DEVICE &&
		qla_scm_chk_throttle_cmd_opcode(sp) ) {
		ret = qla2xxx_throttle_curr_req(&fcport->sfc);
		if (ret == true) {
			atomic_inc(&ha->throttle_write);
			return ret;
		}
	}

	return ret;
}

void
qla2xxx_scmr_manage_qdepth(srb_t *sp, struct fc_port *fcport, bool inc)
{
	int curr;
	struct scsi_qla_host *vha = fcport->vha;

	if (!IS_SCM_CAPABLE(vha->hw))
		return;

	if (inc == true) {
		if (qla_scm_chk_throttle_cmd_opcode(sp)) {
			if (sp->dir == DMA_TO_DEVICE) {
				atomic_inc(&fcport->sfc.perf.dir_q_depth);
				curr = atomic_read(&fcport->sfc.perf.dir_q_depth);
				if (atomic_read(&fcport->sfc.perf.max_q_depth) <
				    curr)
					atomic_set(&fcport->sfc.perf.max_q_depth, curr);
			} else {
				atomic_inc(&vha->hw->sfc.perf.dir_q_depth);
				curr = atomic_read(&vha->hw->sfc.perf.dir_q_depth);
				if (atomic_read(&vha->hw->sfc.perf.max_q_depth) <
				    curr)
					atomic_set(&vha->hw->sfc.perf.max_q_depth, curr);
			}
		}
		atomic_inc(&vha->hw->sfc.perf.q_depth);
		atomic_inc(&fcport->sfc.perf.q_depth);
	} else {
		atomic_dec(&vha->hw->sfc.perf.q_depth);
		atomic_dec(&fcport->sfc.perf.q_depth);
		if (qla_scm_chk_throttle_cmd_opcode(sp)) {
			if (sp->dir == DMA_TO_DEVICE)
				atomic_dec(&fcport->sfc.perf.dir_q_depth);
			else
				atomic_dec(&vha->hw->sfc.perf.dir_q_depth);
		}
	}
}

void
qla2xxx_scmr_cleanup(srb_t *sp, scsi_qla_host_t *vha, struct scsi_cmnd *cmd)
{
	fc_port_t *fcport = (struct fc_port *)cmd->device->hostdata;

	if (!IS_SCM_CAPABLE(vha->hw))
		return;

	atomic_dec(&fcport->sfc.perf.scmr_reqs_per_period);
	qla2xxx_atomic64_sub(&fcport->sfc.perf.scmr_bytes_per_period,
	    scsi_bufflen(cmd));
	atomic_dec(&vha->hw->sfc.perf.scmr_reqs_per_period);
	qla2xxx_atomic64_sub(&vha->hw->sfc.perf.scmr_bytes_per_period,
	    scsi_bufflen(cmd));
	qla2xxx_scmr_manage_qdepth(sp, fcport, false);
}

/*
 * qla2xxx_scmr_flow_control - To rate limit I/O on congestion.
 *
 */
void
qla2xxx_scmr_flow_control(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	struct fc_port *fcport;

	qla2xxx_minute_stats(&ha->sfc);
	/* Controlled at the port level */
	qla2xxx_tune_host_throttle(&ha->sfc);

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
                        continue;

		qla2xxx_minute_stats(&fcport->sfc);
		qla2xxx_handle_tgt_congestion(fcport);
	}
}

void
qla2xxx_scmr_clear_congn(struct qla_scmr_flow_control *sfc)
{
	struct qla_hw_data *ha = sfc->vha->hw;

	qla_scmr_clear_congested(sfc);
	sfc->level = QLA_CONG_NONE;
	sfc->expiration_jiffies = 0;

	/* Clear severity status for the application as well */
	ha->scm.congestion.severity = 0;
	ha->scm.last_event_timestamp = qla_get_real_seconds();
}

void
qla2xxx_scmr_clear_throttle(struct qla_scmr_flow_control *sfc)
{
	if (sfc->mode == QLA_MODE_Q_DEPTH) {
		qla_scmr_clear_throttle_qdepth(sfc);
	}
	atomic_set(&sfc->scmr_base, 0);
	atomic_set(&sfc->scmr_permitted, 0);
	qla2xxx_clear_baseline_tp(sfc);
	sfc->rstats->throttle_cleared++;
	sfc->dir = QLA_DIR_NONE;
	sfc->throttle_period =
	    sfc->event_period + sfc->event_period_buffer;

	ql_dbg(ql_dbg_scm, sfc->vha, 0x0203,
	    "USCM: Clearing Throttling for:%8phN\n", qla_scmr_is_tgt(sfc) ?
			sfc->fcport->port_name: sfc->vha->port_name);
}

void
qla2xxx_update_scm_fcport(scsi_qla_host_t *vha)
{
	fc_port_t *fcport;
	struct qla_scmr_flow_control *sfc;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
			continue;

		sfc = &fcport->sfc;
		if (qla_scmr_test_notify_fw(sfc)) {
			qla2xxx_set_vl(sfc->fcport, fcport->vl.v_lane);
			qla_scmr_clr_notify_fw(sfc);
		}
	}
}

/*
 * Ensure that the caller checks for IS_SCM_CAPABLE()
 */
void
qla2xxx_update_sfc_ios(srb_t *sp, struct qla_hw_data *ha,
		       fc_port_t *fcport, int new)
{
	atomic_inc(&ha->sfc.perf.scmr_reqs_per_period);
	atomic_inc(&fcport->sfc.perf.scmr_reqs_per_period);
	qla2xxx_atomic64_add(&ha->sfc.perf.scmr_bytes_per_period, new);
	qla2xxx_atomic64_add(&fcport->sfc.perf.scmr_bytes_per_period, new);
	qla2xxx_scmr_manage_qdepth(sp, fcport, true);
	return;
}

/*
 * Clear all stats maintained by SCM
 */

void
qla2xxx_clear_scm_stats(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport =  NULL;

	memset(&ha->scm.stats, 0, sizeof(struct qla_scm_stats));
	memset(&ha->scm.sev, 0, sizeof(struct qla_fpin_severity));
	memset(&ha->sig_sev, 0, sizeof(struct qla_sig_severity));
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
			continue;

		memset(&fcport->scm.stats, 0,
		    sizeof(struct qla_scm_stats));
	}

}

void
qla2xxx_clear_scmr_stats(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport =  NULL;

	memset(&ha->scm.rstats, 0, sizeof(struct qla_scmr_stats));
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
			continue;

		memset(&fcport->scm.rstats, 0,
		    sizeof(struct qla_scmr_stats));
	}
}


/*
 * Function Name: qla2xxx_scmr_init_deltas
 *
 * Description:
 * Initialize deltas used to throttle down/up based
 * on the profiles.
 *
 * PARAMETERS:
 * sfc:	USCM flow control
 */
void qla2xxx_scmr_init_deltas(struct qla_scmr_flow_control *sfc)
{
	sfc->scmr_down_delta[QL_SCM_MONITOR] = 0;
	sfc->scmr_down_delta[QL_SCM_CONSERVATIVE] = 1;
	sfc->scmr_down_delta[QL_SCM_MODERATE] = 2;
	sfc->scmr_down_delta[QL_SCM_AGGRESSIVE] = 3;
	sfc->scmr_up_delta[QL_SCM_MONITOR] = 1;
	sfc->scmr_up_delta[QL_SCM_CONSERVATIVE] = 8;
	sfc->scmr_up_delta[QL_SCM_MODERATE] = 4;
	sfc->scmr_up_delta[QL_SCM_AGGRESSIVE] = 2;
}

/* Helper routine to prepare RDF ELS payload.
 * Refer to FC LS 5.01 for a detailed explanation
 */
static void qla_prepare_rdf_payload(scsi_qla_host_t *vha)
{
	vha->rdf_els_payload.els_code = RDF_OPCODE;
	vha->rdf_els_payload.desc_len = cpu_to_be32(sizeof(struct rdf_els_descriptor));
	vha->rdf_els_payload.rdf_desc.desc_tag =
		cpu_to_be32(QLA_ELS_DTAG_FPIN_REGISTER);

	vha->rdf_els_payload.rdf_desc.desc_cnt =
		cpu_to_be32(ELS_RDF_REG_TAG_CNT);

	vha->rdf_els_payload.rdf_desc.desc_len =
		cpu_to_be32(sizeof(struct rdf_els_descriptor) - 8);
	vha->rdf_els_payload.rdf_desc.desc_tags[0] =
		cpu_to_be32(QLA_ELS_DTAG_LNK_INTEGRITY);
	vha->rdf_els_payload.rdf_desc.desc_tags[1] =
		cpu_to_be32(QLA_ELS_DTAG_DELIVERY);
	vha->rdf_els_payload.rdf_desc.desc_tags[2] =
		cpu_to_be32(QLA_ELS_DTAG_PEER_CONGEST);
	vha->rdf_els_payload.rdf_desc.desc_tags[3] =
		cpu_to_be32(QLA_ELS_DTAG_CONGESTION);
	vha->rdf_els_payload.rdf_desc.desc_tags[4] =
		cpu_to_be32(QLA_ELS_DTAG_PUN);
}

/* Helper routine to prepare RDF ELS payload.
 * Refer to FC LS 5.01 for a detailed explanation
 */
static void qla_prepare_edc_payload(scsi_qla_host_t *vha)
{
	struct edc_els_payload *edc = &vha->hw->edc_els_payload;

	edc->els_code = EDC_OPCODE;
	edc->desc_len = cpu_to_be32(sizeof(struct edc_els_descriptor));

	edc->edc_desc.link_fault_cap_descriptor_tag = cpu_to_be32(ELS_EDC_LFC_INFO);
	edc->edc_desc.lfc_descriptor_length = cpu_to_be32(12);

	edc->edc_desc.cong_sig_cap_descriptor_tag = cpu_to_be32(ELS_EDC_CONG_SIG_INFO);
	edc->edc_desc.csc_descriptor_length = cpu_to_be32(16);
}

/*
 * Update various fields of SP to send the ELS via the ELS PT
 * IOCB.
 */

static void qla_update_sp(srb_t *sp, scsi_qla_host_t *vha, u8 cmd)
{
	struct qla_els_pt_arg *a = &sp->u.iocb_cmd.u.drv_els.els_pt_arg;
	struct srb_iocb *iocb_cmd = &sp->u.iocb_cmd;
	void *buf;
	dma_addr_t dma_addr;
	int len;
	u8 al_pa;

	if (cmd == RDF_OPCODE)
		al_pa = 0xFD;
	else
		al_pa = 0xFE;

	a->els_opcode = cmd;
	a->nport_handle = cpu_to_le16(sp->fcport->loop_id);
	a->vp_idx = sp->vha->vp_idx;
	a->control_flags = 0;
	a->rx_xchg_address = 0; //No resp DMA from fabric

	a->did.b.al_pa = al_pa;
	a->did.b.area = 0xFF;
	a->did.b.domain = 0xFF;

	if (cmd == RDF_OPCODE)
		a->tx_len = a->tx_byte_count =
			cpu_to_le32(sizeof(iocb_cmd->u.drv_els.els_req.rdf_cmd));
	else
		a->tx_len = a->tx_byte_count =
			cpu_to_le32(sizeof(iocb_cmd->u.drv_els.els_req.edc_cmd));

	a->rx_len = a->rx_byte_count = cpu_to_le32(sizeof(iocb_cmd->u.drv_els.els_rsp));

	len = iocb_cmd->u.drv_els.dma_addr.cmd_len = sizeof(iocb_cmd->u.drv_els.els_req);

	buf = dma_alloc_coherent(&vha->hw->pdev->dev, len,
		&dma_addr, GFP_KERNEL);

	iocb_cmd->u.drv_els.dma_addr.cmd_buf = buf;
	iocb_cmd->u.drv_els.dma_addr.cmd_dma = dma_addr;

	if (cmd == RDF_OPCODE)
		memcpy(iocb_cmd->u.drv_els.dma_addr.cmd_buf, &vha->rdf_els_payload,
				sizeof(vha->rdf_els_payload));
	else
		memcpy(iocb_cmd->u.drv_els.dma_addr.cmd_buf, &vha->hw->edc_els_payload,
				sizeof(vha->hw->edc_els_payload));

	a->tx_addr = iocb_cmd->u.drv_els.dma_addr.cmd_dma;

	len = iocb_cmd->u.drv_els.dma_addr.rsp_len = sizeof(iocb_cmd->u.drv_els.els_rsp);

	buf = dma_alloc_coherent(&vha->hw->pdev->dev, len,
		&dma_addr, GFP_KERNEL);

	iocb_cmd->u.drv_els.dma_addr.rsp_buf = buf;
	iocb_cmd->u.drv_els.dma_addr.rsp_dma = dma_addr;

	a->rx_addr = iocb_cmd->u.drv_els.dma_addr.rsp_dma;
}

/*
 * qla2xxx_scm_get_features -
 * Get the firmware/Chip related supported features w.r.t SCM
 * Issue mbox 5A.
 * Parse through the response and get relevant values
 */
int
qla2xxx_scm_get_features(scsi_qla_host_t *vha)
{
	dma_addr_t fdma;
	u16 sz = FW_FEATURES_SIZE;
	int rval = 0;
	u8 *f;
	int i;
	u8 scm_feature;
	struct edc_els_descriptor *edc = &vha->hw->edc_els_payload.edc_desc;

	f = dma_alloc_coherent(&vha->hw->pdev->dev, sz,
		&fdma, GFP_KERNEL);
		if (!f) {
			ql_log(ql_log_warn, vha, 0x7035,
				"DMA alloc failed for feature buf.\n");
			return -ENOMEM;
		}

	rval = qla_get_features(vha, fdma, FW_FEATURES_SIZE);
	if (rval != QLA_SUCCESS) {
		ql_log(ql_log_warn, vha, 0x7035,
			"Get features failed :0x%x.\n", rval);
	} else {
		scm_feature = f[0];
		/* If both bits 3 and 4 are zero, firmware sends the ELS
		 * 27xx has bit 4 set and bit 3 cleared
		 */
		if ((!(scm_feature & BIT_3) && !(scm_feature & BIT_4))) {
			rval = 1;
			goto free;
		}
		i = 4;
		if (scm_feature & BIT_3) {
			/* The next 3 words contain Link fault Capability */
			edc->degrade_activate_threshold = get_unaligned_be32(&f[i]);
			i += 4;
			edc->degrade_deactivate_threshold = get_unaligned_be32(&f[i]);
			i += 4;
			edc->fec_degrade_interval = get_unaligned_be32(&f[i]);
			i += 4;
		}
		if (scm_feature & BIT_4) {
			/* The next 4 words contain Cong. Sig. Capability */
			i = 16;
			edc->tx_signal_cap = (get_unaligned_be32(&f[i]));
			i += 4;
			edc->tx_signal_freq = get_unaligned_be32(&f[i]);
			i += 4;
			edc->rx_signal_cap = get_unaligned_be32(&f[i]);
			i += 4;
			edc->rx_signal_freq = get_unaligned_be32(&f[i]);
			i += 4;
		}
	}
free:
	dma_free_coherent(&vha->hw->pdev->dev, sz, f, fdma);
	return rval;
}

void qla2x00_scm_els_sp_done(srb_t *sp, int res)
{
	struct scsi_qla_host *vha = sp->vha;
	struct els_resp *rsp =
		(struct els_resp *)sp->u.iocb_cmd.u.drv_els.dma_addr.rsp_buf;
	u8 err_code;

	if (res == QLA_SUCCESS) {
		ql_log(ql_log_info, vha, 0x700f,
			"%s ELS completed for port:%8phC\n",
			(sp->type == SRB_ELS_EDC)?"EDC":"RDF", vha->port_name);

		if (rsp->resp_code == ELS_LS_RJT) {
			struct fc_els_ls_rjt *rjt =
				(struct fc_els_ls_rjt *)sp->u.iocb_cmd.u.drv_els.dma_addr.rsp_buf;
			err_code = rjt->er_reason;
			ql_log(ql_log_info, vha, 0x503f,
				"%s rejected with code:0x%x\n",(sp->type == SRB_ELS_EDC)?"EDC":"RDF",
					err_code);
			if (sp->type == SRB_ELS_EDC) {
				if (err_code == ELS_RJT_BUSY && ++vha->hw->edc_retry_cnt < MAX_USCM_ELS_RETRIES)
					set_bit(SCM_SEND_EDC, &vha->dpc_flags);
			} else {
				if (err_code == ELS_RJT_BUSY && ++vha->rdf_retry_cnt < MAX_USCM_ELS_RETRIES)
					set_bit(SCM_SEND_RDF, &vha->dpc_flags);
			}
		} else if ((rsp->resp_code == ELS_LS_ACC) && (sp->type == SRB_ELS_RDF)) {
			/* RDF completion indicates that SCM can be supported */
			ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x503f,
				"RDF completed \n");
			vha->hw->flags.scm_enabled = 1;
			vha->hw->scm.scm_fabric_connection_flags |= SCM_FLAG_RDF_COMPLETED;
			/* If VL is negotiated successfully */
			if (vha->hw->flags.conn_fabric_cisco_er_rdy)
				vha->hw->scm.scm_fabric_connection_flags |= SCM_FLAG_CISCO_CONNECTED;
		}
	} else {
		ql_log(ql_log_warn, vha, 0x701a,
			"%s ELS failed for port:%8phC, res:0x%x\n",
			(sp->type == SRB_ELS_EDC)?"EDC":"RDF", vha->port_name, res);
		if (sp->type == SRB_ELS_EDC) {
			if (++vha->hw->edc_retry_cnt < MAX_USCM_ELS_RETRIES) {
				ql_log(ql_log_info, vha, 0x701b,
					"Retrying EDC:retry:%d\n",vha->hw->edc_retry_cnt);
				set_bit(SCM_SEND_EDC, &vha->dpc_flags);
			}
		} else if (sp->type == SRB_ELS_RDF) {
			if (++vha->rdf_retry_cnt < MAX_USCM_ELS_RETRIES) {
				ql_log(ql_log_info, vha, 0x701c,
					"Retrying RDF:retry:%d\n",vha->rdf_retry_cnt);
				set_bit(SCM_SEND_RDF, &vha->dpc_flags);
			}
		}
	}
	/* ref: INIT */
	kref_put(&sp->cmd_kref, qla2x00_sp_release);
}

void qla2x00_scm_els_sp_free(srb_t *sp)
{
	void *cmd_buf, *rsp_buf;
	dma_addr_t cmd_dma, rsp_dma;
	int cmd_len, rsp_len;
	struct qla_work_evt *e;

	cmd_buf = sp->u.iocb_cmd.u.drv_els.dma_addr.cmd_buf;
	cmd_dma = sp->u.iocb_cmd.u.drv_els.dma_addr.cmd_dma;
	cmd_len = sp->u.iocb_cmd.u.drv_els.dma_addr.cmd_len;

	rsp_buf = sp->u.iocb_cmd.u.drv_els.dma_addr.rsp_buf;
	rsp_dma = sp->u.iocb_cmd.u.drv_els.dma_addr.rsp_dma;
	rsp_len = sp->u.iocb_cmd.u.drv_els.dma_addr.rsp_len;

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, sp->vha, 0x700a,
			"cmd_buf:%p, cmd_dma:%llx, len:%d\n",
			cmd_buf, cmd_dma, cmd_len);

	e = qla2x00_alloc_work(sp->vha, QLA_EVT_UNMAP);
	if (!e) {
		dma_free_coherent(&sp->vha->hw->pdev->dev,
				cmd_len,
				cmd_buf,
				cmd_dma);
		cmd_buf = NULL;
		dma_free_coherent(&sp->vha->hw->pdev->dev,
				rsp_len,
				rsp_buf,
				rsp_dma);
		rsp_buf = NULL;
		qla2x00_free_fcport(sp->fcport);
		qla2x00_rel_sp(sp);
	} else {
		e->u.iosb.sp = sp;
		qla2x00_post_work(sp->vha, e);
	}
}

/*
 * qla2xxx_scm_send_rdf_els - Send RDF ELS to the switch
 * Called by both base port and vports
 */

int
qla2xxx_scm_send_rdf_els(scsi_qla_host_t *vha)
{
	srb_t *sp;
	fc_port_t *fcport = NULL;
	int rval = 0;

	/* Allocate a dummy fcport structure, since functions
	 * preparing the IOCB and mailbox command retrieves port
	 * specific information from fcport structure.
	 */

	fcport = qla2x00_alloc_fcport(vha, GFP_KERNEL);
	if (!fcport) {
		rval = -ENOMEM;
		return rval;
	}

	qla_prepare_rdf_payload(vha);

	/* Initialize all required  fields of fcport */
	fcport->vha = vha;
	fcport->d_id = vha->d_id;
	fcport->loop_id = NPH_FABRIC_CONTROLLER; // RDF, EDC -> F_PORT
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x700a,
		"loop-id=%x portid=%-2x%02x%02x.\n",
		fcport->loop_id,
		fcport->d_id.b.domain, fcport->d_id.b.area, fcport->d_id.b.al_pa);

	/* Alloc SRB structure */
	sp = qla2x00_get_sp(vha, fcport, GFP_KERNEL);
	if (!sp) {
		qla2x00_free_fcport(fcport);
		rval = -ENOMEM;
		return rval;
	}

	sp->type = SRB_ELS_RDF;
	sp->name = "rdf_els";
	sp->u.iocb_cmd.u.drv_els.els_req.rdf_cmd = vha->rdf_els_payload;

	qla_update_sp(sp, vha, RDF_OPCODE);

	sp->free = qla2x00_scm_els_sp_free;
	sp->done = qla2x00_scm_els_sp_done;

	/* Reset scm_enabled to indicate SCM is not yet enabled */
	vha->hw->flags.scm_enabled = 0;
	vha->hw->scm.scm_fabric_connection_flags &= ~SCM_FLAG_RDF_COMPLETED;

	rval = qla2x00_start_sp(sp);
	if (rval != QLA_SUCCESS) {
		ql_log(ql_log_warn, vha, 0x700e,
		    "qla2x00_start_sp failed = %d\n", rval);
		qla2x00_rel_sp(sp);
		qla2x00_free_fcport(fcport);
		rval = -EIO;
	}
	return rval;
}

/*
 * qla2xxx_scm_send_edc_els - Send EDC ELS to the switch
 * Called by base port - post the initial login to the fabric
 */
int
qla2xxx_scm_send_edc_els(scsi_qla_host_t *vha)
{
	srb_t *sp;
	fc_port_t *fcport = NULL;
	int rval = 0;

	/* Allocate a dummy fcport structure, since functions
	 * preparing the IOCB and mailbox command retrieves port
	 * specific information from fcport structure.
	 */
	fcport = qla2x00_alloc_fcport(vha, GFP_KERNEL);
	if (!fcport) {
		rval = -ENOMEM;
		return rval;
	}

	qla_prepare_edc_payload(vha);

	/* Initialize all required  fields of fcport */
	fcport->vha = vha;
	fcport->d_id = vha->d_id;
	fcport->loop_id = NPH_F_PORT;
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x700a,
	    "loop-id=%x "
	    "portid=%-2x%02x%02x.\n",
	    fcport->loop_id,
	    fcport->d_id.b.domain, fcport->d_id.b.area, fcport->d_id.b.al_pa);

	/* Alloc SRB structure */
	sp = qla2x00_get_sp(vha, fcport, GFP_KERNEL);
	if (!sp) {
		qla2x00_free_fcport(fcport);
		rval = -ENOMEM;
		return rval;
	}

	sp->type = SRB_ELS_EDC;
	sp->name = "edc_els";
	sp->u.iocb_cmd.u.drv_els.els_req.edc_cmd = vha->hw->edc_els_payload;

	qla_update_sp(sp, vha, EDC_OPCODE);

	sp->free = qla2x00_scm_els_sp_free;
	sp->done = qla2x00_scm_els_sp_done;

	rval = qla2x00_start_sp(sp);
	if (rval != QLA_SUCCESS) {
		ql_log(ql_log_warn, vha, 0x700e,
		    "qla2x00_start_sp failed = %d\n", rval);
		qla2x00_rel_sp(sp);
		rval = -EIO;
		qla2x00_free_fcport(fcport);
	}
	return rval;
}

void qla2xxx_send_uscm_els(scsi_qla_host_t *vha)
{
	if (test_and_clear_bit(SCM_SEND_EDC, &vha->dpc_flags)) {
		ql_log(ql_log_info, vha, 0x20ad,
		    "Driver sending EDC for port :%8phC\n", vha->port_name);
		qla2xxx_scm_send_edc_els(vha);
	}
	if (test_and_clear_bit(SCM_SEND_RDF, &vha->dpc_flags)) {
		ql_log(ql_log_info, vha, 0x20ae,
		    "Driver sending RDF for port :%8phC\n", vha->port_name);
		qla2xxx_scm_send_rdf_els(vha);
	}
}

/*
 * Function Name: qla2xxx_prepare_els_rsp
 *
 * Description:
 * Helper function to populate common fields in the ELS response
 *
 * PARAMETERS:
 * rsp_els: The ELS response to be sent
 * purex: ELS request received by the HBA
 */
static void qla2xxx_prepare_els_rsp(struct els_entry_24xx *rsp_els,
		struct purex_entry_24xx *purex)
{
	rsp_els->entry_type = ELS_IOCB_TYPE;
	rsp_els->entry_count = 1;
	rsp_els->sys_define = 0;
	rsp_els->entry_status = 0;
	rsp_els->handle = QLA_SKIP_HANDLE;
	rsp_els->nport_handle = purex->nport_handle;
	rsp_els->tx_dsd_count = 1;
	rsp_els->vp_index = purex->vp_idx;
	rsp_els->sof_type = EST_SOFI3;
	rsp_els->rx_xchg_address = purex->rx_xchg_addr;
	rsp_els->rx_dsd_count = 0;
	rsp_els->opcode = purex->els_frame_payload[0];

	rsp_els->d_id[0] = purex->s_id[0];
	rsp_els->d_id[1] = purex->s_id[1];
	rsp_els->d_id[2] = purex->s_id[2];

	rsp_els->control_flags = cpu_to_le16(EPD_ELS_ACC);

}
/*
 * Function Name: qla2xx_scm_process_purex_edc
 *
 * Description:
 * Prepare an EDC response and send it to the switch
 *
 * PARAMETERS:
 * vha:	SCSI qla host
 * purex: EDC ELS received by HBA
 */
void
qla2xx_scm_process_purex_edc(struct scsi_qla_host *vha,
			  struct purex_item *item)
{
	struct qla_hw_data *ha = vha->hw;
	struct purex_entry_24xx *purex =
	    (struct purex_entry_24xx *)&item->iocb;
	struct edc_els_resp_payload *edc_rsp_payload = ha->edc_rsp_payload;
	dma_addr_t edc_rsp_payload_dma = ha->edc_rsp_payload_dma;
	struct els_entry_24xx *rsp_els = NULL;
	uint edc_rsp_payload_length = sizeof(*edc_rsp_payload);

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0181,
	    "-------- ELS REQ -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0182,
	    purex, sizeof(*purex));

	rsp_els = __qla2x00_alloc_iocbs(item->qpair, NULL);
	if (!rsp_els) {
		ql_log(ql_log_warn, vha, 0x018b,
		    "Failed to allocate iocbs for ELS RSP \n");
		return;
	}

	/* Prepare Response IOCB */
	qla2xxx_prepare_els_rsp(rsp_els, purex);

	rsp_els->rx_byte_count = 0;
	rsp_els->tx_byte_count = edc_rsp_payload_length;

	rsp_els->tx_address = edc_rsp_payload_dma;
	rsp_els->tx_len = rsp_els->tx_byte_count;

	rsp_els->rx_address = 0;
	rsp_els->rx_len = 0;

	edc_rsp_payload->resp_code = cpu_to_be32(ELS_LS_ACC << 24); /* LS_ACC */
	/* Send Link Service Req Info desc also */
	edc_rsp_payload->desc_len = cpu_to_be32(sizeof(struct edc_els_descriptor) +
			sizeof(struct edc_els_link_srv_descriptor));
	edc_rsp_payload->edc_ls_desc.link_srv_info_descriptor_tag =
		cpu_to_be32(QLA_ELS_DTAG_LS_REQ_INFO);
	edc_rsp_payload->edc_ls_desc.ls_info_descriptor_length =
		cpu_to_be32(4);
	edc_rsp_payload->edc_ls_desc.ls_info_descriptor_length =
		cpu_to_be32(EDC_OPCODE);

	/* Prepare Response Payload */
	memcpy(&edc_rsp_payload->edc_desc,
			&vha->hw->edc_els_payload.edc_desc, sizeof(struct edc_els_descriptor));

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0183,
	    "Sending ELS Response to EDC Request...\n");
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0184,
	    "-------- ELS RSP -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0185,
	    rsp_els, sizeof(*rsp_els));
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0186,
	    "-------- ELS RSP PAYLOAD -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0187,
	    edc_rsp_payload, edc_rsp_payload_length);

	wmb();
	qla2x00_start_iocbs(vha, item->qpair->req);

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x018a, "%s: done.\n", __func__);

}
/*
 * Function Name: qla2xxx_scm_process_purex_rdf
 *
 * Description:
 * Prepare an RDF response and send it to the switch
 *
 * PARAMETERS:
 * vha:	SCSI qla host
 * purex: RDF ELS received by HBA
 */
void
qla2xxx_scm_process_purex_rdf(struct scsi_qla_host *vha,
			  struct purex_item *item)
{
	struct purex_entry_24xx *purex =
	    (struct purex_entry_24xx *)&item->iocb;
	dma_addr_t rdf_payload_dma = vha->rdf_payload_dma;
	struct els_entry_24xx *rsp_els = NULL;
	struct rdf_els_payload *rdf_payload = vha->rdf_payload;
	uint rdf_payload_length = sizeof(*rdf_payload);

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0191,
	    "-------- ELS REQ -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0192,
	    purex, sizeof(*purex));

	rsp_els = __qla2x00_alloc_iocbs(item->qpair, NULL);
	if (!rsp_els) {
		ql_log(ql_log_warn, vha, 0x019c,
		    "Failed to allocate dma buffer ELS RSP.\n");
		return;
	}

	/* Prepare Response IOCB */
	qla2xxx_prepare_els_rsp(rsp_els, purex);

	rsp_els->rx_byte_count = 0;
	rsp_els->tx_byte_count = rdf_payload_length - ELS_RDF_RSP_DESC_LEN;
	/* Since we send 1 desc */

	rsp_els->tx_address = rdf_payload_dma;
	rsp_els->tx_len = rsp_els->tx_byte_count;

	rsp_els->rx_address = 0;
	rsp_els->rx_len = 0;

	/* Prepare Response Payload */
	/* For Nx ports, the desc list len will be 12
	 * and the LS req info will be sent as part of LS_ACC.
	 * An RDF will be re-sent to the switch
	 */
	rdf_payload->els_code = cpu_to_be32(ELS_LS_ACC << 24); /* LS_ACC */
	rdf_payload->desc_len = cpu_to_be32(ELS_RDF_RSP_DESC_LEN);
	rdf_payload->rdf_desc.desc_tag = cpu_to_be32(QLA_ELS_DTAG_LS_REQ_INFO);
	rdf_payload->rdf_desc.desc_cnt = cpu_to_be32(1);
	rdf_payload->rdf_desc.desc_len = cpu_to_be32(4);
	rdf_payload->rdf_desc.desc_tags[0] = cpu_to_be32(RDF_OPCODE << 24);


	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0193,
	    "Sending ELS Response to incoming RDF...\n");
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0194,
	    "-------- ELS RSP -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0195,
	    rsp_els, sizeof(*rsp_els));
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x0196,
	    "-------- ELS RSP PAYLOAD -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x0197,
	    rdf_payload, rdf_payload_length);

	wmb();
	qla2x00_start_iocbs(vha, item->qpair->req);

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x019a, "%s: done.\n", __func__);

	/* Schedule an RDF */
	set_bit(SCM_SEND_RDF, &vha->dpc_flags);
}

/*
 * Function Name: qla2xxx_alloc_rdf_payload
 *
 * Description:
 * Allocate DMA memory for RDF payload. Called for base and vports
 *
 * PARAMETERS:
 * vha:	SCSI qla host
 */
void qla2xxx_scm_alloc_rdf_payload(struct scsi_qla_host *vha)
{
	struct qla_hw_data *ha = vha->hw;

	vha->rdf_payload = dma_alloc_coherent(&ha->pdev->dev, sizeof(struct rdf_els_payload),
	    &vha->rdf_payload_dma, GFP_KERNEL);
	if (!vha->rdf_payload) {
		ql_log(ql_log_warn, vha, 0x019b,
		    "Failed allocate dma buffer ELS RSP payload.\n");
	}
	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x019b, "%s: rdf_payload:%px \n",
			__func__, vha->rdf_payload);
}

/*
 * Function Name: qla2xxx_free_rdf_payload
 *
 * Description:
 * Free DMA memory for RDF payload. Called for base and vports
 *
 * PARAMETERS:
 * vha:	SCSI qla host
 */
void qla2xxx_scm_free_rdf_payload(struct scsi_qla_host *vha)
{
	struct qla_hw_data *ha = vha->hw;

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x019c, "%s: Freeing:%px\n",
			__func__, vha->rdf_payload);
	if (vha->rdf_payload)
		dma_free_coherent(&ha->pdev->dev, sizeof(struct rdf_els_payload),
		    vha->rdf_payload, vha->rdf_payload_dma);
}

/* Clear all events when clearing SCM Stats */
void
qla_scm_clear_previous_event(struct scsi_qla_host *vha)
{
	fc_port_t *fcport = NULL;
	struct qla_hw_data *ha = vha->hw;

	ha->scm.current_events = SCM_EVENT_NONE;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		fcport->scm.current_events = SCM_EVENT_NONE;
	}
}

/* Move all targets to NORMAL VL.
 * Called on a port bounce/ISP reset
 */
void
qla_scm_host_clear_vl_state(struct scsi_qla_host *vha)
{
	fc_port_t *fcport = NULL;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		qla_scm_tgt_clear_vl_state(fcport);
	}
}

void
qla_scm_tgt_clear_vl_state(fc_port_t *fcport)
{
	struct scsi_qla_host *vha = fcport->vha;

	fcport->vl.v_lane = VL_NORMAL;
	fcport->vl.prio_hi = vha->hw->flogi_acc.rx_vl[VL_NORMAL].prio_hi;
	fcport->vl.prio_lo = vha->hw->flogi_acc.rx_vl[VL_NORMAL].prio_lo;
}

/* Clear all SCM related state and throttling for all the targets */
void
qla_scm_clear_all_tgt_sess(struct scsi_qla_host *vha)
{
       fc_port_t *fcport = NULL;

       list_for_each_entry(fcport, &vha->vp_fcports, list) {
               qla_scm_clear_session(fcport);
       }
}

/* Clear all SCM related stats/state and throttling for the remote port */
void
qla_scm_clear_session(fc_port_t *fcport)
{
	if (fcport->vha->hw->flags.scm_enabled) {
		qla2xxx_scmr_clear_congn(&fcport->sfc);
		qla2xxx_scmr_clear_throttle(&fcport->sfc);
		qla_scm_tgt_clear_vl_state(fcport);
		fcport->scm.current_events = SCM_EVENT_NONE;
	}
}

/* Clear all SCM related state and throttling for the host */
void
qla_scm_clear_host(struct scsi_qla_host *vha)
{
	if (vha->hw->flags.scm_enabled) {
		qla2xxx_scmr_clear_congn(&vha->hw->sfc);
		qla2xxx_scmr_clear_throttle(&vha->hw->sfc);
		qla_scm_clear_previous_event(vha);
		qla_scm_host_clear_vl_state(vha);
	}
}

bool
qla_scm_chk_throttle_cmd_opcode(srb_t *sp)
{
	struct srb_iocb *nvme;
	struct nvmefc_fcp_req *fd;
	struct nvme_fc_cmd_iu *cmd;

	if (sp->type == SRB_SCSI_CMD) {
		switch (sp->u.scmd.cmd->cmnd[0]) {
			case WRITE_10:
			case WRITE_12:
			case WRITE_16:
			case READ_10:
			case READ_12:
			case READ_16:
				return true;
			break;
			default:
				return false;
		}
	} else if (sp->type == SRB_NVME_CMD) {
		nvme = &sp->u.iocb_cmd;
		fd = nvme->u.nvme.desc;
		cmd = fd->cmdaddr;

		if (cmd->sqe.rw.opcode == nvme_cmd_write ||
			cmd->sqe.rw.opcode == nvme_cmd_read)
		return true;
	}
	return false;
}

uint8_t
qla_get_throttling_state(struct qla_scmr_flow_control *sfc)
{
	uint8_t io_throttle = 0;

	if (sfc->profile.scmr_profile == 0) /* Monitor */
		io_throttle = QLA_THROTTLE_DISABLED;
	else {
		if (sfc->dir == QLA_DIR_NONE) /* Not throttled */
			io_throttle = QLA_THROTTLE_NONE;
		else
			io_throttle = QLA_THROTTLE_ACTIVE;
	}

	return io_throttle;
}

DECLARE_ENUM2STR_LOOKUP(qla_get_li_event_type, ql_fpin_li_event_types,
			QL_FPIN_LI_EVT_TYPES_INIT);
static void
qla_link_integrity_tgt_stats_update(struct fpin_descriptor *fpin_desc,
				    fc_port_t *fcport)
{
	uint16_t event;
	uint32_t event_count;
	const char * li_type;

	event = be16_to_cpu(fpin_desc->link_integrity.event_type);
	event_count = be32_to_cpu(fpin_desc->link_integrity.event_count);
	li_type = qla_get_li_event_type(event);
	ql_log(ql_log_info, fcport->vha, 0x502d,
	       "Link Integrity Event Type: %s(%x) for Port %8phN\n",
		li_type, event, fcport->port_name);

	fcport->scm.link_integrity.event_type = event;
	fcport->scm.link_integrity.event_modifier =
	    be16_to_cpu(fpin_desc->link_integrity.event_modifier);
	fcport->scm.link_integrity.event_threshold =
	    be32_to_cpu(fpin_desc->link_integrity.event_threshold);
	fcport->scm.link_integrity.event_count = event_count;
	fcport->scm.last_event_timestamp = qla_get_real_seconds();

	fcport->scm.current_events |= SCM_EVENT_LINK_INTEGRITY;
	switch (event) {
	case QL_FPIN_LI_UNKNOWN:
		fcport->scm.stats.li_failure_unknown += event_count;
		break;
	case QL_FPIN_LI_LINK_FAILURE:
		fcport->scm.stats.li_link_failure_count += event_count;
		break;
	case QL_FPIN_LI_LOSS_OF_SYNC:
		fcport->scm.stats.li_loss_of_sync_count += event_count;
		break;
	case QL_FPIN_LI_LOSS_OF_SIG:
		fcport->scm.stats.li_loss_of_signals_count += event_count;
		break;
	case QL_FPIN_LI_PRIM_SEQ_ERR:
		fcport->scm.stats.li_prim_seq_err_count += event_count;
		break;
	case QL_FPIN_LI_INVALID_TX_WD:
		fcport->scm.stats.li_invalid_tx_word_count += event_count;
		break;
	case QL_FPIN_LI_INVALID_CRC:
		fcport->scm.stats.li_invalid_crc_count += event_count;
		break;
	case QL_FPIN_LI_UNCORRECTABLE_FEC:
		fcport->scm.li_uncorrectable_fec_count += event_count;
		break;
	case QL_FPIN_LI_DEVICE_SPEC:
		fcport->scm.stats.li_device_specific += event_count;
		break;
	}
}

static void
qla_link_integrity_host_stats_update(struct fpin_descriptor *fpin_desc,
				    struct qla_hw_data *ha)
{
	uint16_t event;
	uint32_t event_count;
	const char *li_type;

	event = be16_to_cpu(fpin_desc->link_integrity.event_type);
	event_count = be32_to_cpu(fpin_desc->link_integrity.event_count);
	li_type = qla_get_li_event_type(event);

	ha->scm.link_integrity.event_type = event;
	ha->scm.link_integrity.event_modifier =
	    be16_to_cpu(fpin_desc->link_integrity.event_modifier);
	ha->scm.link_integrity.event_threshold =
	    be32_to_cpu(fpin_desc->link_integrity.event_threshold);
	ha->scm.link_integrity.event_count = event_count;
	ha->scm.last_event_timestamp = qla_get_real_seconds();

	ha->scm.current_events |= SCM_EVENT_LINK_INTEGRITY;
	switch (event) {
	case QL_FPIN_LI_UNKNOWN:
		ha->scm.stats.li_failure_unknown += event_count;
		break;
	case QL_FPIN_LI_LINK_FAILURE:
		ha->scm.stats.li_link_failure_count += event_count;
		break;
	case QL_FPIN_LI_LOSS_OF_SYNC:
		ha->scm.stats.li_loss_of_sync_count += event_count;
		break;
	case QL_FPIN_LI_LOSS_OF_SIG:
		ha->scm.stats.li_loss_of_signals_count += event_count;
		break;
	case QL_FPIN_LI_PRIM_SEQ_ERR:
		ha->scm.stats.li_prim_seq_err_count += event_count;
		break;
	case QL_FPIN_LI_INVALID_TX_WD:
		ha->scm.stats.li_invalid_tx_word_count += event_count;
		break;
	case QL_FPIN_LI_INVALID_CRC:
		ha->scm.stats.li_invalid_crc_count += event_count;
		break;
	case QL_FPIN_LI_UNCORRECTABLE_FEC:
		ha->scm.li_uncorrectable_fec_count += event_count;
		break;
	case QL_FPIN_LI_DEVICE_SPEC:
		ha->scm.stats.li_device_specific += event_count;
		break;
	}
}


static void
qla_scm_process_link_integrity_d(struct scsi_qla_host *vha,
				 struct fpin_descriptor *fpin_desc,
				 int length)
{
	uint16_t event;
	const char * li_type;
	fc_port_t *fcport =  NULL;
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *d_fcport = NULL, *a_fcport = NULL;

	fcport = qla2x00_find_fcport_by_wwpn(vha,
				fpin_desc->link_integrity.detecting_port_name,
				0);
	if (fcport) {
		d_fcport = fcport;
		qla_link_integrity_tgt_stats_update(fpin_desc, fcport);
	}

	fcport = qla2x00_find_fcport_by_wwpn(vha,
				fpin_desc->link_integrity.attached_port_name,
				0);
	if (fcport) {
		a_fcport = fcport;
		qla_link_integrity_tgt_stats_update(fpin_desc, fcport);
	}

	if (memcmp(vha->port_name, fpin_desc->link_integrity.attached_port_name,
		   WWN_SIZE) == 0) {
		event = be16_to_cpu(fpin_desc->link_integrity.event_type);
		li_type = qla_get_li_event_type(event);
		ql_log(ql_log_info, vha, 0x5093,
		       "Link Integrity Event Type: %s(%x) for HBA WWN %8phN\n",
		       li_type, event, vha->port_name);

		qla_link_integrity_host_stats_update(fpin_desc, ha);
	}
}

DECLARE_ENUM2STR_LOOKUP_DELI_EVENT

static void
qla_delivery_tgt_stats_update(struct fpin_descriptor *fpin_desc,
			      fc_port_t *fcport)
{
	uint32_t event;
	const char * deli_type;

	event = be32_to_cpu(fpin_desc->delivery.delivery_reason_code);
	deli_type =  qla_get_dn_event_type(event);
	ql_log(ql_log_info, fcport->vha, 0x5095,
	       "Delivery Notification Reason Code: %s(%x) for Port %8phN\n",
	       deli_type, event, fcport->port_name);

	fcport->scm.current_events |= SCM_EVENT_DELIVERY;
	fcport->scm.delivery.delivery_reason =
		    be32_to_cpu(fpin_desc->delivery.delivery_reason_code);
	switch (event) {
	case FPIN_DELI_UNKNOWN:
		fcport->scm.stats.dn_unknown++;
		break;
	case FPIN_DELI_TIMEOUT:
		fcport->scm.stats.dn_timeout++;
		break;
	case FPIN_DELI_UNABLE_TO_ROUTE:
		fcport->scm.stats.dn_unable_to_route++;
		break;
	case FPIN_DELI_DEVICE_SPEC:
		fcport->scm.stats.dn_device_specific++;
		break;
	}
	fcport->scm.last_event_timestamp = qla_get_real_seconds();
}

static void
qla_delivery_host_stats_update(struct fpin_descriptor *fpin_desc,
			      struct qla_hw_data *ha)
{
	uint32_t event;
	const char *deli_type;

	event = be32_to_cpu(fpin_desc->delivery.delivery_reason_code);
	deli_type =  qla_get_dn_event_type(event);

	ha->scm.current_events |= SCM_EVENT_DELIVERY;
	ha->scm.delivery.delivery_reason =
		    be32_to_cpu(fpin_desc->delivery.delivery_reason_code);
	switch (event) {
	case FPIN_DELI_UNKNOWN:
		ha->scm.stats.dn_unknown++;
		break;
	case FPIN_DELI_TIMEOUT:
		ha->scm.stats.dn_timeout++;
		break;
	case FPIN_DELI_UNABLE_TO_ROUTE:
		ha->scm.stats.dn_unable_to_route++;
		break;
	case FPIN_DELI_DEVICE_SPEC:
		ha->scm.stats.dn_device_specific++;
		break;
	}
	ha->scm.last_event_timestamp = qla_get_real_seconds();
}

/*
 * Process Delivery Notification Descriptor
 */
static void
qla_scm_process_delivery_notification_d(struct scsi_qla_host *vha,
					struct fpin_descriptor *fpin_desc)
{
	uint32_t event;
	const char * deli_type;
	fc_port_t *fcport =  NULL;
	struct qla_hw_data *ha = vha->hw;

	fcport = qla2x00_find_fcport_by_wwpn(vha,
				fpin_desc->delivery.detecting_port_name, 0);
	if (fcport)
		qla_delivery_tgt_stats_update(fpin_desc, fcport);

	fcport = qla2x00_find_fcport_by_wwpn(vha,
				fpin_desc->delivery.attached_port_name, 0);
	if (fcport)
		qla_delivery_tgt_stats_update(fpin_desc, fcport);

	if (memcmp(vha->port_name, fpin_desc->delivery.attached_port_name,
		   WWN_SIZE) == 0) {
		event = be32_to_cpu(fpin_desc->delivery.delivery_reason_code);
		deli_type =  qla_get_dn_event_type(event);
		ql_log(ql_log_info, vha, 0x5096,
		       "Delivery Notification Reason Code: %s(%x) for HBA WWN %8phN\n",
		       deli_type, event, vha->port_name);
		qla_delivery_host_stats_update(fpin_desc, ha);
	}
}

static void
qla_scm_set_target_device_state(fc_port_t *fcport,
				struct fpin_descriptor *fpin_desc)
{
	struct qla_scmr_flow_control *sfc = &fcport->sfc;
	u64 delta;

	sfc->throttle_period = sfc->event_period_buffer + sfc->event_period;
	delta = (2 * sfc->event_period * HZ);
	if (delta < HZ)
		delta = HZ;

	switch (be16_to_cpu(fpin_desc->peer_congestion.event_type)) {
	case FPIN_CONGN_CLEAR:
		atomic_set(&sfc->scmr_congn_signal, QLA_SIG_CLEAR);
		ql_log(ql_log_info, fcport->vha, 0x5097,
		       "Port %8phN Slow Device: Cleared\n", fcport->port_name);
		break;
	case FPIN_CONGN_LOST_CREDIT:
		break;
	case FPIN_CONGN_CREDIT_STALL:
		sfc->expiration_jiffies = jiffies + delta;
		atomic_set(&sfc->scmr_congn_signal, QLA_SIG_CREDIT_STALL);
		ql_log(ql_log_info, fcport->vha, 0x508c,
		       "Port %8phN Slow Device: Set\n", fcport->port_name);
		break;
	case FPIN_CONGN_OVERSUBSCRIPTION:
		sfc->expiration_jiffies = jiffies + delta;
		atomic_set(&sfc->scmr_congn_signal, QLA_SIG_OVERSUBSCRIPTION);
		ql_log(ql_log_info, fcport->vha, 0x508c,
		       "Port %8phN Slow Device: Set\n", fcport->port_name);
		break;
	default:
		break;

	}
}

DECLARE_ENUM2STR_LOOKUP_CONGN_EVENT

static void
qla_peer_congestion_tgt_stats_update(struct fpin_descriptor *fpin_desc,
				     fc_port_t *fcport)
{
	uint16_t event;
	uint32_t event_period_secs = 0;
	const char * congn_type;

	event = be16_to_cpu(fpin_desc->peer_congestion.event_type);
	congn_type = qla_get_congn_event_type(event);
	ql_log(ql_log_info, fcport->vha, 0x5098,
	       "Peer Congestion Event Type: %s(%x) for Port %8phN\n",
	       congn_type, event, fcport->port_name);

	fcport->scm.last_event_timestamp = qla_get_real_seconds();
	fcport->scm.peer_congestion.event_type = event;
	fcport->scm.peer_congestion.event_modifier =
	    be16_to_cpu(fpin_desc->peer_congestion.event_modifier);
	fcport->scm.peer_congestion.event_period =
	    be32_to_cpu(fpin_desc->peer_congestion.event_period);
	event_period_secs =
	    be32_to_cpu(fpin_desc->peer_congestion.event_period) / 1000;
	if (event_period_secs)
		fcport->sfc.event_period = event_period_secs;
	else
		fcport->sfc.event_period = 1;

	fcport->sfc.event_period_buffer = QLA_SCMR_BUFFER;

	// What is the API to get system time ?
	fcport->scm.current_events |= SCM_EVENT_PEER_CONGESTION;
	switch (event) {
	case FPIN_CONGN_CLEAR:
		fcport->scm.stats.cn_clear++;
		break;
	case FPIN_CONGN_LOST_CREDIT:
		fcport->scm.stats.cn_lost_credit++;
		break;
	case FPIN_CONGN_CREDIT_STALL:
		fcport->scm.stats.cn_credit_stall++;
		break;
	case FPIN_CONGN_OVERSUBSCRIPTION:
		fcport->scm.stats.cn_oversubscription++;
		break;
	case FPIN_CONGN_DEVICE_SPEC:
		fcport->scm.stats.cn_device_specific++;
		break;
	}
	qla_scm_set_target_device_state(fcport, fpin_desc);
}

/*
 * Process Peer-Congestion Notification Descriptor
 */
static void
qla_scm_process_peer_congestion_notification_d(struct scsi_qla_host *vha,
					struct fpin_descriptor *fpin_desc,
					int length)
{
	fc_port_t *fcport =  NULL;
	fc_port_t *d_fcport = NULL, *a_fcport = NULL;

	fcport = qla2x00_find_fcport_by_wwpn(vha,
			fpin_desc->peer_congestion.detecting_port_name, 0);
	if (fcport) {
		d_fcport = fcport;
		qla_peer_congestion_tgt_stats_update(fpin_desc, fcport);
	}

	fcport = qla2x00_find_fcport_by_wwpn(vha,
			fpin_desc->peer_congestion.attached_port_name, 0);
	if (fcport) {
		a_fcport = fcport;
		qla_peer_congestion_tgt_stats_update(fpin_desc, fcport);
	}
}

/*
 * qla_scm_process_pun_notification_d() -
 * Process Priority Update Notification Descriptor
 */

static void
qla_scm_process_pun_notification_d(struct scsi_qla_host *vha,
					  struct fpin_descriptor *fpin_desc)
{
	uint32_t event_period_secs = 0;
	uint32_t num_prio_mappings = 0;
	int i, j;
	uint16_t num_devices;
	uint8_t pr_low;
	uint8_t pr_high;
	uint8_t port_name[WWN_SIZE];
	fc_port_t *fcport =  NULL;
	struct pun_wwn_list *plist;

	event_period_secs =
	    be32_to_cpu(fpin_desc->pun.event_period) / 1000;
	num_prio_mappings =
	    be32_to_cpu(fpin_desc->pun.num_prio_map_records);
	for (i = 0; i < num_prio_mappings; i++) {
		num_devices = be16_to_cpu(fpin_desc->pun.prio_map_record.num_devices);
		pr_low = fpin_desc->pun.prio_map_record.pr_low;
		pr_high = fpin_desc->pun.prio_map_record.pr_high;
		plist = &fpin_desc->pun.prio_map_record.port_list;
		for (j = 0; j < num_devices; j++) {
			memcpy(port_name, plist->port_name, WWN_SIZE);
			fcport = qla2x00_find_fcport_by_wwpn(vha, port_name, 0);
			if (fcport) {
				fcport->scm.stats.pun_count++;
				fcport->vl.prio_hi = pr_high;
				fcport->vl.prio_lo = pr_low;
				ql_log(ql_log_info, vha, 0x5099,
				       "Prio range for %8phN, Low:0x%x, High: 0x%x\n",
				       fcport->port_name, fcport->vl.prio_lo, fcport->vl.prio_hi);
			} else {
				ql_log(ql_log_warn, vha, 0x5099,
				       "PUN for invalid port %8phN \n",
				       port_name);
			}
			plist++;
		}

		fpin_desc += sizeof(struct priority_map_record);
	}
}

/*
 * qla_scm_process_congestion_notification_d() - Process
 * Process Congestion Notification Descriptor
 * @rsp: response queue
 * @pkt: Entry pointer
 */
static void
qla_scm_process_congestion_notification_d(struct scsi_qla_host *vha,
					  struct fpin_descriptor *fpin_desc)
{
	u64 delta;
	uint16_t event;
	uint32_t event_period_secs = 0;
	const char * congn_type;
	struct qla_hw_data *ha = vha->hw;
	struct qla_scmr_flow_control *sfc = &ha->sfc;


	event = be16_to_cpu(fpin_desc->congestion.event_type);
	congn_type = qla_get_congn_event_type(event);
	ql_log(ql_log_info, vha, 0x5099,
	       "Congestion Event Type: %s(%x)\n", congn_type, event);

	ha->scm.congestion.event_type = event;
	ha->scm.congestion.event_modifier =
	    be16_to_cpu(fpin_desc->congestion.event_modifier);
	ha->scm.congestion.event_period =
	    be32_to_cpu(fpin_desc->congestion.event_period);
	event_period_secs =
	    be32_to_cpu(fpin_desc->congestion.event_period) / 1000;
	if (event_period_secs)
		sfc->event_period = event_period_secs;
	else
		sfc->event_period = 1;
	ha->scm.last_event_timestamp = qla_get_real_seconds();

	ha->scm.congestion.severity =
	    fpin_desc->congestion.severity;

	sfc->throttle_period = sfc->event_period_buffer + sfc->event_period;
	delta = (2 * sfc->event_period * HZ);

	ha->scm.current_events |= SCM_EVENT_CONGESTION;
	switch (be16_to_cpu(fpin_desc->congestion.event_type)) {
	case FPIN_CONGN_CLEAR:
		atomic_set(&sfc->scmr_congn_signal, QLA_SIG_CLEAR);
		ha->scm.stats.cn_clear++;
		break;
	case FPIN_CONGN_CREDIT_STALL:
		if (qla_scmr_get_sig(sfc) == QLA_SIG_NONE) {
			atomic_set(&sfc->scmr_congn_signal,
			    QLA_SIG_CREDIT_STALL);
			sfc->expiration_jiffies = jiffies + delta;
		}
		ha->scm.stats.cn_credit_stall++;
		break;
	case FPIN_CONGN_OVERSUBSCRIPTION:
		if (qla_scmr_get_sig(sfc) == QLA_SIG_NONE) {
			atomic_set(&sfc->scmr_congn_signal,
			    QLA_SIG_OVERSUBSCRIPTION);
			sfc->expiration_jiffies = jiffies + delta;
		}
		ha->scm.stats.cn_oversubscription++;
		break;
	case FPIN_CONGN_LOST_CREDIT:
		ha->scm.stats.cn_lost_credit++;
		break;
	default:
		break;
	}

	if (fpin_desc->congestion.severity ==
	    SCM_CONGESTION_SEVERITY_WARNING) {
		sfc->level = QLA_CONG_LOW;
		ha->scm.sev.cn_warning++;
	} else if (fpin_desc->congestion.severity ==
	    SCM_CONGESTION_SEVERITY_ERROR) {
		sfc->level = QLA_CONG_HIGH;
		ha->scm.sev.cn_alarm++;
	}
}

void qla27xx_process_purex_fpin(struct scsi_qla_host *vha, struct purex_item *item)
{
	struct fpin_descriptor *fpin_desc;
	uint16_t fpin_desc_len, total_fpin_desc_len;
	uint32_t fpin_offset = 0;
	void *pkt = &item->iocb;
	uint16_t pkt_size = item->size;

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x508d,
	       "%s: Enter\n", __func__);

	ql_dbg(ql_dbg_scm + ql_dbg_verbose, vha, 0x508e,
	       "-------- ELS REQ -------\n");
	ql_dump_buffer(ql_dbg_scm + ql_dbg_verbose, vha, 0x508f,
		       pkt, pkt_size);

	fpin_desc = (struct fpin_descriptor *)((uint8_t *)pkt +
	    FPIN_ELS_DESCRIPTOR_LIST_OFFSET);

	fpin_desc_len = pkt_size - FPIN_ELS_DESCRIPTOR_LIST_OFFSET;
	if (fpin_desc_len != be32_to_cpu(fpin_desc->descriptor_length) + 8) {
		ql_dbg(ql_dbg_scm, vha, 0x5099,
		    "desc len=%d, actual len=%d\n",
		    be32_to_cpu(fpin_desc->descriptor_length), fpin_desc_len);
	}

	total_fpin_desc_len = 0;
	while (fpin_offset <= fpin_desc_len) {
		fpin_desc = (struct fpin_descriptor *)((uint8_t *)fpin_desc +
		    fpin_offset);
		if (fpin_desc_len < (total_fpin_desc_len + FPIN_DESCRIPTOR_HEADER_SIZE)) {
			ql_dbg(ql_dbg_scm, vha, 0x5099,
			    "fpin_desc_len =%d, total_fpin_desc_len =%d\n",
			    fpin_desc_len, total_fpin_desc_len);
			break;
		}

		switch (be32_to_cpu(fpin_desc->descriptor_tag)) {
		case SCM_NOTIFICATION_TYPE_LINK_INTEGRITY:
			qla_scm_process_link_integrity_d(vha,
							 fpin_desc,
							 fpin_desc_len);
			break;
		case SCM_NOTIFICATION_TYPE_DELIVERY:
			qla_scm_process_delivery_notification_d(vha, fpin_desc);
			break;
		case SCM_NOTIFICATION_TYPE_PEER_CONGESTION:
			qla_scm_process_peer_congestion_notification_d(vha,
								fpin_desc,
								fpin_desc_len);
			break;
		case SCM_NOTIFICATION_TYPE_CONGESTION:
			qla_scm_process_congestion_notification_d(vha,
								fpin_desc);
			break;
		case SCM_NOTIFICATION_TYPE_PUN:
			qla_scm_process_pun_notification_d(vha, fpin_desc);
			break;
		}
		fpin_offset = be32_to_cpu(fpin_desc->descriptor_length) +
			FPIN_ELS_DESCRIPTOR_LIST_OFFSET;
		total_fpin_desc_len += fpin_offset;
	}
	ql_fc_host_fpin_rcv(vha->host, pkt_size, (char *)pkt, 0);
}

void qla_scm_update_profile(struct scsi_qla_host *vha, uint8_t new_profile)
{
	fc_port_t *fcport;

	vha->hw->sfc.profile.scmr_profile = new_profile;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (!(fcport->port_type & FCT_TARGET) &&
		    !(fcport->port_type & FCT_NVME_TARGET))
                        continue;
		fcport->sfc.profile.scmr_profile = new_profile;
	}
}
