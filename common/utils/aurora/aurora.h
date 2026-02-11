/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use
 * this file except in compliance with the License.
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

#ifndef AURORA_H
#define AURORA_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ─────────────────────────────────────────────────────── */
#define AURORA_VERSION 1
#define AURORA_SHM_PATH "/aurora_shm"
#define AURORA_SHM_DIR "/aurora-ipc"

/* ── Node Types ──────────────────────────────────────────────────── */
typedef enum {
  AURORA_NODE_CU = 0,
  AURORA_NODE_DU = 1,
  AURORA_NODE_RU = 2,
  AURORA_NODE_MONOLITHIC = 3,
  AURORA_NODE_TYPE_COUNT
} aurora_node_type_t;

static inline const char *aurora_node_type_str(aurora_node_type_t t)
{
  static const char *names[] = {"CU", "DU", "RU", "MONOLITHIC"};
  return (t < AURORA_NODE_TYPE_COUNT) ? names[t] : "UNKNOWN";
}

/* ── Metric limits ───────────────────────────────────────────────── */
#define AURORA_MAX_WORKERS 32
#define AURORA_MAX_UES 128
#define AURORA_WORKER_NAME_LEN 32
#define AURORA_MAX_NODE_METRICS 64
#define AURORA_MAX_TRAFFIC_METRICS 32
#define AURORA_METRIC_NAME_LEN 48
#define AURORA_SHM_MAGIC 0x4155524F /* "AURO" */

/* ── Raw Metric Types ────────────────────────────────────────────── */
typedef enum {
  AURORA_METRIC_COUNTER = 0,
  AURORA_METRIC_GAUGE,
  AURORA_METRIC_HISTOGRAM_BIN,
  AURORA_METRIC_RAW_VALUE
} aurora_metric_type_t;

/* ── Single Raw Metric ───────────────────────────────────────────── */
typedef struct {
  char name[AURORA_METRIC_NAME_LEN];
  aurora_metric_type_t type;
  int64_t value;
} aurora_raw_metric_t;

/* ── Worker Resource Metrics (from eBPF) ─────────────────────────── */
typedef struct {
  char name[AURORA_WORKER_NAME_LEN];
  uint64_t cpu_time_ns;
  uint64_t sched_switches;
  uint64_t sched_wakeups;
  uint64_t mem_alloc_bytes;
  uint64_t net_tx_bytes;
  uint64_t net_rx_bytes;
  uint64_t vfs_read_bytes;
  uint64_t vfs_write_bytes;
  bool active;
} aurora_worker_metrics_t;

/* ── Traffic Table Metrics ───────────────────────────────────────── */
typedef struct {
  char name[AURORA_METRIC_NAME_LEN];
  uint64_t packet_count;
  uint64_t byte_count;
  int64_t last_packet_size;
} aurora_traffic_metric_t;

/* ── Shared Memory Header ────────────────────────────────────────── */
typedef struct {
  uint32_t magic;
  uint32_t version;
  aurora_node_type_t node_type;
  uint64_t timestamp_ns;
  uint32_t num_node_metrics;
  uint32_t num_workers;
  uint32_t num_traffic_metrics;
  pthread_mutex_t mutex;
} aurora_shm_header_t;

/* ── Full Shared Memory Layout ───────────────────────────────────── */
typedef struct {
  aurora_shm_header_t header;
  aurora_raw_metric_t node_metrics[AURORA_MAX_NODE_METRICS];
  aurora_worker_metrics_t workers[AURORA_MAX_WORKERS];
  aurora_traffic_metric_t traffic[AURORA_MAX_TRAFFIC_METRICS];
} aurora_shm_t;

/* ── Producer API ────────────────────────────────────────────────── */

/**
 * Initialize the Aurora producer with the given node type.
 * Creates and maps shared memory, initializes the robust mutex.
 * @param node_type The type of RAN node (CU, DU, RU, MONOLITHIC)
 * @return Pointer to the shared memory region, or NULL on failure
 */
aurora_shm_t *aurora_producer_init(aurora_node_type_t node_type);

/**
 * Lock the shared memory mutex for writing.
 * Uses PTHREAD_MUTEX_ROBUST; handles EOWNERDEAD by making consistent.
 * @return 0 on success, -1 on failure
 */
int aurora_producer_lock(aurora_shm_t *shm);

/**
 * Unlock the shared memory mutex after writing.
 * @return 0 on success, -1 on failure
 */
int aurora_producer_unlock(aurora_shm_t *shm);

/**
 * Write a single raw metric into the node metrics block.
 * Must be called while holding the lock.
 * @return 0 on success, -1 if block is full
 */
int aurora_producer_write_metric(aurora_shm_t *shm, const char *name,
                                 aurora_metric_type_t type, int64_t value);

/**
 * Write worker resource metrics from eBPF data.
 * Must be called while holding the lock.
 * @return 0 on success, -1 if block is full
 */
int aurora_producer_write_worker(aurora_shm_t *shm, const aurora_worker_metrics_t *w);

/**
 * Write a traffic table metric.
 * Must be called while holding the lock.
 * @return 0 on success, -1 if block is full
 */
int aurora_producer_write_traffic(aurora_shm_t *shm, const char *name,
                                  uint64_t pkt_count, uint64_t byte_count,
                                  int64_t last_pkt_size);

/**
 * Update the timestamp to current monotonic clock.
 * Must be called while holding the lock, typically at end of write cycle.
 */
void aurora_producer_update_timestamp(aurora_shm_t *shm);

/**
 * Clean up the producer: unmap and unlink shared memory.
 */
void aurora_producer_destroy(aurora_shm_t *shm);

#ifdef __cplusplus
}
#endif

#endif /* AURORA_H */
