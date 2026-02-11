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
 * @file aurora_e2_metrics.h
 * @brief E2 interface KPM (Key Performance Metrics) collection
 *
 * Collects metrics exposed through the E2 interface following 3GPP TS 28.522
 * measurement names. Supports both simulated and live data sources.
 *
 * Supported KPM measurements:
 *   - DRB.PdcpSduVolumeDL (Megabits)
 *   - DRB.PdcpSduVolumeUL (Megabits)
 *   - DRB.RlcSduDelayDl (microseconds)
 *   - DRB.UEThpDl (kbps)
 *   - DRB.UEThpUl (kbps)
 *   - RRU.PrbTotDl (percent)
 *   - RRU.PrbTotUl (percent)
 */

#pragma once

#include "aurora_metrics_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Raw E2 KPM metrics snapshot
 *
 * All volume/throughput metrics are monotonic counters.
 * Delay and PRB utilization are gauges.
 */
typedef struct {
  /* Monotonic counters (for delta computation) */
  double pdcp_sdu_vol_dl;      /**< DRB.PdcpSduVolumeDL total (Megabits, monotonic) */
  double pdcp_sdu_vol_ul;      /**< DRB.PdcpSduVolumeUL total (Megabits, monotonic) */

  /* Gauge metrics (instantaneous) */
  double rlc_sdu_delay_dl;     /**< DRB.RlcSduDelayDl (microseconds) */
  double ue_thp_dl;            /**< DRB.UEThpDl (kbps) */
  double ue_thp_ul;            /**< DRB.UEThpUl (kbps) */
  double prb_tot_dl;           /**< RRU.PrbTotDl (percent) */
  double prb_tot_ul;           /**< RRU.PrbTotUl (percent) */

  struct timespec timestamp;   /**< Collection timestamp (nanosec precision) */
  bool valid;                  /**< Whether this snapshot contains valid data */
} AuroraE2RawMetrics;

/**
 * @brief Collect raw E2 KPM metrics
 *
 * Reads current KPM metrics from the E2 interface data source.
 * When compiled without AURORA_LIVE_RAN, returns simulated values.
 * When linked against the live RAN, reads from PDCP/RLC/MAC stats.
 *
 * @param node_name Name to identify this E2 node (e.g. "gnb_du_0")
 * @param node_type Type of node (CU, DU, or gNB)
 * @param raw Output: raw E2 KPM metrics
 * @return 0 on success, -1 on failure
 */
int aurora_e2_collect_raw(const char *node_name,
                          AuroraNodeType node_type,
                          AuroraE2RawMetrics *raw);

/**
 * @brief Compute delta E2 metrics and write to shared memory
 *
 * Writes gauge metrics directly. For monotonic counters (PDCP volumes),
 * computes deltas if previous snapshot is available.
 *
 * @param handle Shared memory handle
 * @param node_name Name of the E2 node
 * @param node_type Type of node (CU, DU, or gNB)
 * @param current Current raw metrics
 * @param previous Previous raw metrics (NULL for first collection)
 * @return 0 on success, -1 on failure
 */
int aurora_e2_write_metrics(void *handle,
                            const char *node_name,
                            AuroraNodeType node_type,
                            const AuroraE2RawMetrics *current,
                            const AuroraE2RawMetrics *previous);

#ifdef __cplusplus
}
#endif
