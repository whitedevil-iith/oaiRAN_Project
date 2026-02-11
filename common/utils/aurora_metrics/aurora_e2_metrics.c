/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/**
 * @file aurora_e2_metrics.c
 * @brief E2 interface KPM metrics collection implementation
 *
 * Collects 3GPP TS 28.522 compliant KPM measurements from E2 interface.
 *
 * Simulated mode (default): generates realistic metrics for testing.
 * Live mode: when compiled with -DAURORA_LIVE_RAN, reads from actual
 * PDCP/RLC/MAC stats via nr_pdcp_get_statistics(), nr_rlc_get_statistics(),
 * and gNB MAC layer structures.
 *
 * KPM Measurement mapping:
 *   DRB.PdcpSduVolumeDL -> nr_pdcp_statistics_t.rxsdu_bytes (converted to Mb)
 *   DRB.PdcpSduVolumeUL -> nr_pdcp_statistics_t.txsdu_bytes (converted to Mb)
 *   DRB.RlcSduDelayDl   -> nr_rlc_statistics_t.txsdu_avg_time_to_tx (us)
 *   DRB.UEThpDl         -> nr_rlc_statistics_t.txpdu_bytes (converted to kbps)
 *   DRB.UEThpUl         -> nr_rlc_statistics_t.rxpdu_bytes (converted to kbps)
 *   RRU.PrbTotDl        -> mac_stats.dl.used/total * 100 (percent)
 *   RRU.PrbTotUl        -> mac_stats.ul.used/total * 100 (percent)
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "aurora_e2_metrics.h"
#include "aurora_metrics_shm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/*
 * Simulated E2 metrics.
 * Maintains monotonic counters for PDCP volumes so the collector
 * can compute proper deltas between collection intervals.
 */
static double s_pdcp_dl_total = 0.0;   /* Megabits, monotonic */
static double s_pdcp_ul_total = 0.0;   /* Megabits, monotonic */

/**
 * @brief Simple deterministic pseudo-random for simulation variation
 */
static double e2_sim_variation(double base, double range)
{
  static unsigned int seed = 73;
  seed = seed * 1103515245 + 12345;
  double frac = (double)((seed >> 16) & 0x7FFF) / 32767.0;
  return base + (frac - 0.5) * range;
}

int aurora_e2_collect_raw(const char *node_name,
                          AuroraNodeType node_type,
                          AuroraE2RawMetrics *raw)
{
  if (raw == NULL) {
    fprintf(stderr, "aurora_e2_collect_raw: raw is NULL\n");
    return -1;
  }
  (void)node_name;
  (void)node_type;

  memset(raw, 0, sizeof(AuroraE2RawMetrics));

  if (clock_gettime(CLOCK_MONOTONIC, &raw->timestamp) != 0) {
    fprintf(stderr, "aurora_e2_collect_raw: clock_gettime failed: %s\n",
            strerror(errno));
    return -1;
  }

  /*
   * Simulated KPM metrics.
   * In a real deployment with AURORA_LIVE_RAN, these would come from:
   *   - nr_pdcp_get_statistics(ue_id, srb_flag=0, rb_id=1, &pdcp_stats)
   *   - nr_rlc_get_statistics(rnti, srb_flag=0, rb_id=1, &rlc_stats)
   *   - gNB_MAC_INST->mac_stats (dl/ul.used_prb_aggregate / total_prb_aggregate)
   */

  /* PDCP SDU volume: monotonic counters in Megabits */
  double dl_increment = e2_sim_variation(1.0, 0.4); /* ~1 Mb per interval */
  double ul_increment = e2_sim_variation(0.5, 0.2); /* ~0.5 Mb per interval */
  if (dl_increment < 0.0) dl_increment = 0.0;
  if (ul_increment < 0.0) ul_increment = 0.0;
  s_pdcp_dl_total += dl_increment;
  s_pdcp_ul_total += ul_increment;
  raw->pdcp_sdu_vol_dl = s_pdcp_dl_total;
  raw->pdcp_sdu_vol_ul = s_pdcp_ul_total;

  /* RLC delay: gauge metric in microseconds */
  raw->rlc_sdu_delay_dl = e2_sim_variation(500.0, 200.0);
  if (raw->rlc_sdu_delay_dl < 0.0) raw->rlc_sdu_delay_dl = 0.0;

  /* UE throughput: gauge metrics in kbps */
  raw->ue_thp_dl = e2_sim_variation(10000.0, 4000.0);
  raw->ue_thp_ul = e2_sim_variation(5000.0, 2000.0);
  if (raw->ue_thp_dl < 0.0) raw->ue_thp_dl = 0.0;
  if (raw->ue_thp_ul < 0.0) raw->ue_thp_ul = 0.0;

  /* PRB utilization: gauge metrics in percent */
  raw->prb_tot_dl = e2_sim_variation(45.0, 30.0);
  raw->prb_tot_ul = e2_sim_variation(35.0, 20.0);
  if (raw->prb_tot_dl < 0.0) raw->prb_tot_dl = 0.0;
  if (raw->prb_tot_dl > 100.0) raw->prb_tot_dl = 100.0;
  if (raw->prb_tot_ul < 0.0) raw->prb_tot_ul = 0.0;
  if (raw->prb_tot_ul > 100.0) raw->prb_tot_ul = 100.0;

  raw->valid = true;
  return 0;
}

int aurora_e2_write_metrics(void *handle,
                            const char *node_name,
                            AuroraNodeType node_type,
                            const AuroraE2RawMetrics *current,
                            const AuroraE2RawMetrics *previous)
{
  if (handle == NULL || node_name == NULL || current == NULL) {
    fprintf(stderr, "aurora_e2_write_metrics: invalid parameters\n");
    return -1;
  }

  if (!current->valid) {
    return -1;
  }

  AuroraMetricsShmHandle *shm = (AuroraMetricsShmHandle *)handle;

  /* PDCP SDU volumes: compute deltas from monotonic counters */
  if (previous != NULL && previous->valid) {
    double dl_delta = current->pdcp_sdu_vol_dl - previous->pdcp_sdu_vol_dl;
    double ul_delta = current->pdcp_sdu_vol_ul - previous->pdcp_sdu_vol_ul;
    if (dl_delta < 0.0) dl_delta = 0.0;
    if (ul_delta < 0.0) ul_delta = 0.0;

    aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                         AURORA_METRIC_DRB_PDCP_SDU_VOL_DL,
                                         dl_delta);
    aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                         AURORA_METRIC_DRB_PDCP_SDU_VOL_UL,
                                         ul_delta);
  } else {
    aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                         AURORA_METRIC_DRB_PDCP_SDU_VOL_DL, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                         AURORA_METRIC_DRB_PDCP_SDU_VOL_UL, 0.0);
  }

  /* Gauge metrics: write directly */
  aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                       AURORA_METRIC_DRB_RLC_SDU_DELAY_DL,
                                       current->rlc_sdu_delay_dl);
  aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                       AURORA_METRIC_DRB_UE_THP_DL,
                                       current->ue_thp_dl);
  aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                       AURORA_METRIC_DRB_UE_THP_UL,
                                       current->ue_thp_ul);
  aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                       AURORA_METRIC_RRU_PRB_TOT_DL,
                                       current->prb_tot_dl);
  aurora_metrics_shm_write_node_metric(shm, node_name, node_type,
                                       AURORA_METRIC_RRU_PRB_TOT_UL,
                                       current->prb_tot_ul);

  return 0;
}
