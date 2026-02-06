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
 * @brief Worker process metrics collection using eBPF-style direct syscalls
 */

#pragma once

#include "aurora_metrics_types.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Raw monotonic worker metrics (for delta computation)
 */
typedef struct {
  double cpu_time_total;       /**< Total CPU seconds (monotonic) */
  double memory_rss;           /**< RSS memory in bytes (gauge) */
  double memory_heap;          /**< Heap memory in bytes (gauge) */
  double net_tx_bytes_total;   /**< Total network TX bytes (monotonic) */
  double net_rx_bytes_total;   /**< Total network RX bytes (monotonic) */
  double fs_read_bytes_total;  /**< Total FS read bytes (monotonic) */
  double fs_write_bytes_total; /**< Total FS write bytes (monotonic) */
  struct timespec timestamp;   /**< Collection timestamp (nanosec precision) */
} AuroraWorkerRawMetrics;

/**
 * @brief Read CPU time for a worker process (fast using getrusage)
 * 
 * Uses getrusage(RUSAGE_SELF, ...) for fast CPU time without parsing /proc.
 * 
 * @param pid Process ID (use getpid() for current process)
 * @param cpu_time Output: Total CPU time in seconds (monotonic)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_cpu_fast(pid_t pid, double *cpu_time);

/**
 * @brief Read CPU time for a worker process (fallback using /proc)
 * 
 * Fallback method using /proc/[pid]/stat when getrusage cannot be used
 * for other processes.
 * 
 * @param pid Process ID
 * @param cpu_time Output: Total CPU time in seconds (monotonic)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_cpu_proc(pid_t pid, double *cpu_time);

/**
 * @brief Read memory usage for a worker process
 * 
 * @param pid Process ID
 * @param rss_bytes Output: Resident Set Size in bytes (gauge)
 * @param heap_bytes Output: Heap memory in bytes (gauge)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_memory(pid_t pid, double *rss_bytes, double *heap_bytes);

/**
 * @brief Read network statistics for a worker process
 * 
 * @param pid Process ID
 * @param tx_bytes Output: Total transmitted bytes (monotonic)
 * @param rx_bytes Output: Total received bytes (monotonic)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_network(pid_t pid, double *tx_bytes, double *rx_bytes);

/**
 * @brief Read I/O statistics for a worker process
 * 
 * @param pid Process ID
 * @param read_bytes Output: Total bytes read from storage (monotonic)
 * @param write_bytes Output: Total bytes written to storage (monotonic)
 * @return 0 on success, -1 on failure
 */
int aurora_worker_read_io(pid_t pid, double *read_bytes, double *write_bytes);

/**
 * @brief Collect all raw (monotonic) worker metrics
 * 
 * Collects all raw monotonic values. The collector will compute deltas
 * from these raw values.
 * 
 * @param pid Process ID
 * @param raw Output: Raw monotonic metrics
 * @return 0 on success, -1 on failure
 */
int aurora_worker_collect_raw(pid_t pid, AuroraWorkerRawMetrics *raw);

#ifdef __cplusplus
}
#endif
