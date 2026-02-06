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
 * @file aurora_worker_metrics.h
 * @brief Worker process metrics collection via /proc filesystem
 */

#pragma once

#include "aurora_metrics_types.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read CPU time for a worker process
 * 
 * @param pid Process ID
 * @param cpu_time Output: CPU time in seconds
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_cpu(pid_t pid, double *cpu_time);

/**
 * @brief Read memory usage for a worker process
 * 
 * @param pid Process ID
 * @param rss_bytes Output: Resident Set Size in bytes
 * @param heap_bytes Output: Heap memory in bytes
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_memory(pid_t pid, double *rss_bytes, double *heap_bytes);

/**
 * @brief Read network statistics for a worker process
 * 
 * @param pid Process ID
 * @param tx_bytes Output: Transmitted bytes
 * @param rx_bytes Output: Received bytes
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_network(pid_t pid, double *tx_bytes, double *rx_bytes);

/**
 * @brief Read I/O statistics for a worker process
 * 
 * @param pid Process ID
 * @param read_bytes Output: Bytes read from storage
 * @param write_bytes Output: Bytes written to storage
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_io(pid_t pid, double *read_bytes, double *write_bytes);

/**
 * @brief Collect all worker metrics
 * 
 * @param pid Process ID
 * @param entry Output: Worker metric entry (worker_id and worker_name should be pre-filled)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_collect_all(pid_t pid, AuroraWorkerMetricEntry *entry);

#ifdef __cplusplus
}
#endif
