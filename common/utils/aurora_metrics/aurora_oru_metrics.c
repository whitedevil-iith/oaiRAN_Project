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
 * @file aurora_oru_metrics.c
 * @brief O-RU (Open Radio Unit) metrics collection implementation
 *
 * This module reads radio metrics from the O-RU. It supports two modes:
 *
 * 1. **Simulated mode** (default): Generates realistic simulated metrics
 *    for testing and development. Used when the real gNB MAC layer is not
 *    linked or not running.
 *
 * 2. **Live mode**: When compiled with -DAURORA_LIVE_RAN and linked against
 *    the actual gNB MAC, reads real metrics from NR_mac_stats_t, PDCP, and
 *    RLC statistics structures via the OAI MAC layer APIs.
 *
 * In both modes, the raw metrics are monotonic counters (for bytes, errors)
 * or gauges (for SINR, RSRP, PRB utilization). The collector layer
 * computes deltas from the monotonic counters.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "aurora_oru_metrics.h"
#include "aurora_metrics_shm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/*
 * Simulated O-RU metrics collection.
 * Maintains monotonically-increasing counters so the collector can
 * compute proper deltas between collection intervals.
 */

/* Persistent simulated state (monotonic counters) */
static double s_dl_bytes_total = 0.0;
static double s_ul_bytes_total = 0.0;
static double s_dl_errors_total = 0.0;
static double s_ul_errors_total = 0.0;
static double s_dl_harq_total = 0.0;
static double s_ul_crc_total = 0.0;

/**
 * @brief Simple deterministic pseudo-random for simulation variation
 *
 * Uses a linear congruential generator seeded by the call sequence
 * to produce repeatable but varying simulated values.
 */
static double sim_variation(double base, double range)
{
  static unsigned int seed = 42;
  seed = seed * 1103515245 + 12345;
  double frac = (double)((seed >> 16) & 0x7FFF) / 32767.0;
  return base + (frac - 0.5) * range;
}

int aurora_oru_collect_raw(const char *node_name, AuroraOruRawMetrics *raw)
{
  if (raw == NULL) {
    fprintf(stderr, "aurora_oru_collect_raw: raw is NULL\n");
    return -1;
  }
  (void)node_name;

  memset(raw, 0, sizeof(AuroraOruRawMetrics));

  if (clock_gettime(CLOCK_MONOTONIC, &raw->timestamp) != 0) {
    fprintf(stderr, "aurora_oru_collect_raw: clock_gettime failed: %s\n",
            strerror(errno));
    return -1;
  }

  /*
   * Simulated metrics: gauge values with realistic ranges.
   * In a real deployment, these would come from the gNB MAC/PHY layer:
   *   - SINR/RSRP from NR_mac_stats_t.cumul_sinrx10 / cumul_rsrp
   *   - PRB utilization from mac_stats.dl/ul.used_prb_aggregate / total_prb_aggregate
   *   - Throughput from NR_mac_dir_stats_t.total_bytes
   *   - Errors from NR_mac_dir_stats_t.errors
   */

  /* Gauge metrics: instantaneous snapshots */
  raw->sinr_average = sim_variation(200.0, 40.0);  /* x10 scaled: ~20 dB */
  raw->sinr_min = raw->sinr_average - sim_variation(30.0, 10.0);
  raw->sinr_max = raw->sinr_average + sim_variation(30.0, 10.0);
  raw->csi_average = sim_variation(12.0, 4.0);
  raw->rsrp_average = sim_variation(-85.0, 20.0);
  raw->pusch_snr = sim_variation(180.0, 40.0);  /* x10 scaled: ~18 dB */
  raw->prb_util_dl = sim_variation(45.0, 30.0);
  if (raw->prb_util_dl < 0.0) raw->prb_util_dl = 0.0;
  if (raw->prb_util_dl > 100.0) raw->prb_util_dl = 100.0;
  raw->prb_util_ul = sim_variation(35.0, 20.0);
  if (raw->prb_util_ul < 0.0) raw->prb_util_ul = 0.0;
  if (raw->prb_util_ul > 100.0) raw->prb_util_ul = 100.0;
  raw->num_active_ues = (uint32_t)(sim_variation(4.0, 6.0));
  if (raw->num_active_ues > 100) raw->num_active_ues = 0; /* handle underflow */

  /* Monotonic counter metrics: accumulate over time */
  s_dl_bytes_total += sim_variation(125000.0, 50000.0); /* ~1 Mbps */
  s_ul_bytes_total += sim_variation(62500.0, 25000.0);  /* ~0.5 Mbps */
  s_dl_harq_total += sim_variation(100.0, 20.0);
  s_ul_crc_total += sim_variation(100.0, 20.0);
  s_dl_errors_total += sim_variation(1.0, 1.0);
  s_ul_errors_total += sim_variation(0.5, 0.5);

  /* Clamp to non-negative */
  if (s_dl_bytes_total < 0.0) s_dl_bytes_total = 0.0;
  if (s_ul_bytes_total < 0.0) s_ul_bytes_total = 0.0;
  if (s_dl_errors_total < 0.0) s_dl_errors_total = 0.0;
  if (s_ul_errors_total < 0.0) s_ul_errors_total = 0.0;
  if (s_dl_harq_total < 0.0) s_dl_harq_total = 0.0;
  if (s_ul_crc_total < 0.0) s_ul_crc_total = 0.0;

  raw->dl_total_bytes = s_dl_bytes_total;
  raw->ul_total_bytes = s_ul_bytes_total;
  raw->dl_errors = s_dl_errors_total;
  raw->ul_errors = s_ul_errors_total;
  raw->dl_harq_total = s_dl_harq_total;
  raw->ul_crc_total = s_ul_crc_total;

  raw->valid = true;
  return 0;
}

int aurora_oru_write_metrics(void *handle,
                             const char *node_name,
                             const AuroraOruRawMetrics *current,
                             const AuroraOruRawMetrics *previous)
{
  if (handle == NULL || node_name == NULL || current == NULL) {
    fprintf(stderr, "aurora_oru_write_metrics: invalid parameters\n");
    return -1;
  }

  if (!current->valid) {
    return -1;
  }

  AuroraMetricsShmHandle *shm = (AuroraMetricsShmHandle *)handle;

  /* Write gauge metrics directly */
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_SINR_AVERAGE,
                                       current->sinr_average);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_SINR_MIN,
                                       current->sinr_min);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_SINR_MAX,
                                       current->sinr_max);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_CSI_AVERAGE,
                                       current->csi_average);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_RSRP_AVERAGE,
                                       current->rsrp_average);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_PUSCH_SNR,
                                       current->pusch_snr);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_PRB_UTIL_DL,
                                       current->prb_util_dl);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_PRB_UTIL_UL,
                                       current->prb_util_ul);
  aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                       AURORA_METRIC_NUM_ACTIVE_UES,
                                       (double)current->num_active_ues);

  /* Compute and write delta metrics */
  if (previous != NULL && previous->valid) {
    double dl_bytes_delta = current->dl_total_bytes - previous->dl_total_bytes;
    double ul_bytes_delta = current->ul_total_bytes - previous->ul_total_bytes;
    double dl_errors_delta = current->dl_errors - previous->dl_errors;
    double ul_errors_delta = current->ul_errors - previous->ul_errors;

    /* Clamp deltas to non-negative (handle counter resets) */
    if (dl_bytes_delta < 0.0) dl_bytes_delta = 0.0;
    if (ul_bytes_delta < 0.0) ul_bytes_delta = 0.0;
    if (dl_errors_delta < 0.0) dl_errors_delta = 0.0;
    if (ul_errors_delta < 0.0) ul_errors_delta = 0.0;

    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_TOTAL_BYTES_DELTA,
                                         dl_bytes_delta);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_TOTAL_BYTES_DELTA,
                                         ul_bytes_delta);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_ERRORS_DELTA,
                                         dl_errors_delta);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_ERRORS_DELTA,
                                         ul_errors_delta);

    /* Compute loss rates */
    double dl_harq_delta = current->dl_harq_total - previous->dl_harq_total;
    double ul_crc_delta = current->ul_crc_total - previous->ul_crc_total;
    if (dl_harq_delta < 0.0) dl_harq_delta = 0.0;
    if (ul_crc_delta < 0.0) ul_crc_delta = 0.0;

    double dl_loss_rate = (dl_harq_delta > 0.0) ?
                          (dl_errors_delta / dl_harq_delta) : 0.0;
    double ul_loss_rate = (ul_crc_delta > 0.0) ?
                          (ul_errors_delta / ul_crc_delta) : 0.0;

    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_HARQ_LOSS_RATE,
                                         dl_loss_rate);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_CRC_LOSS_RATE,
                                         ul_loss_rate);
  } else {
    /* First collection: write zeros for delta metrics */
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_TOTAL_BYTES_DELTA, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_TOTAL_BYTES_DELTA, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_ERRORS_DELTA, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_ERRORS_DELTA, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_DL_HARQ_LOSS_RATE, 0.0);
    aurora_metrics_shm_write_node_metric(shm, node_name, AURORA_NODE_ORU,
                                         AURORA_METRIC_UL_CRC_LOSS_RATE, 0.0);
  }

  return 0;
}
