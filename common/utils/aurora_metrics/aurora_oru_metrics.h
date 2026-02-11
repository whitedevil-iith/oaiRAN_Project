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
 * @file aurora_oru_metrics.h
 * @brief O-RU (Open Radio Unit) metrics collection
 *
 * Collects radio metrics from O-RU devices including actual radios (e.g. USRP)
 * and simulated radio environments. Metrics include SINR, RSRP, PRB utilization,
 * throughput, HARQ/CRC error rates, and active UE counts.
 *
 * The raw metrics are collected as monotonic counters where appropriate,
 * and the collector layer computes deltas per collection interval.
 */

#pragma once

#include "aurora_metrics_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Raw O-RU metrics snapshot (monotonic counters + gauges)
 *
 * Gauge metrics (SINR, RSRP, PRB utilization, active UEs) are instantaneous.
 * Counter metrics (bytes, errors) are monotonic totals for delta computation.
 */
typedef struct {
  /* Gauge metrics (instantaneous values) */
  double sinr_average;       /**< Average SINR across UEs (dB, x10 scaled) */
  double sinr_min;           /**< Minimum SINR across UEs (dB, x10 scaled) */
  double sinr_max;           /**< Maximum SINR across UEs (dB, x10 scaled) */
  double csi_average;        /**< Average CSI across UEs */
  double rsrp_average;       /**< Average RSRP (dBm) */
  double pusch_snr;          /**< PUSCH SNR (dB, x10 scaled) */
  double prb_util_dl;        /**< DL PRB utilization (percent) */
  double prb_util_ul;        /**< UL PRB utilization (percent) */
  uint32_t num_active_ues;   /**< Number of active UEs */

  /* Monotonic counter metrics (for delta computation) */
  double dl_total_bytes;     /**< Total DL bytes (monotonic) */
  double ul_total_bytes;     /**< Total UL bytes (monotonic) */
  double dl_errors;          /**< Total DL HARQ errors (monotonic) */
  double ul_errors;          /**< Total UL CRC errors (monotonic) */
  double dl_harq_total;      /**< Total DL HARQ attempts (monotonic) */
  double ul_crc_total;       /**< Total UL CRC checks (monotonic) */

  struct timespec timestamp; /**< Collection timestamp (nanosec precision) */
  bool valid;                /**< Whether this snapshot contains valid data */
} AuroraOruRawMetrics;

/**
 * @brief Collect raw O-RU metrics
 *
 * Reads current radio metrics from the underlying hardware or simulation.
 * Returns gauge values directly and monotonic counters for delta computation.
 *
 * If no real radio hardware is available (e.g. running in standalone or
 * test mode), this function returns simulated metrics with valid=true.
 *
 * @param node_name Name to identify this O-RU node
 * @param raw Output: raw O-RU metrics snapshot
 * @return 0 on success, -1 on failure
 */
int aurora_oru_collect_raw(const char *node_name, AuroraOruRawMetrics *raw);

/**
 * @brief Compute delta O-RU metrics and write to shared memory
 *
 * Given current and previous raw snapshots, computes delta values for
 * monotonic counters and writes all metrics (gauges + deltas) to shared
 * memory using the node metric API.
 *
 * @param handle Shared memory handle
 * @param node_name Name of the O-RU node
 * @param current Current raw metrics
 * @param previous Previous raw metrics (NULL for first collection)
 * @return 0 on success, -1 on failure
 */
int aurora_oru_write_metrics(void *handle,
                             const char *node_name,
                             const AuroraOruRawMetrics *current,
                             const AuroraOruRawMetrics *previous);

#ifdef __cplusplus
}
#endif
