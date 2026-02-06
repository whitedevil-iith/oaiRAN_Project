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
 * @file aurora_metrics_shm.h
 * @brief Shared memory management for Aurora metrics
 */

#pragma once

#include "aurora_metrics_types.h"
#include <pthread.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AURORA_SHM_MAGIC 0x41524F52  /* "AROR" */
#define AURORA_SHM_VERSION 1

/**
 * @brief Node metric entry in shared memory
 */
typedef struct {
  AuroraNodeType node_type;               /**< Type of node */
  char node_name[AURORA_MAX_NAME_LEN];    /**< Node name */
  bool active;                            /**< Is this entry active? */
  uint32_t num_metrics;                   /**< Number of metrics stored */
  AuroraMetricValue metrics[AURORA_MAX_METRICS_PER_NODE]; /**< Metric values */
} AuroraNodeMetricEntry;

/**
 * @brief Shared memory header structure
 */
typedef struct {
  uint32_t magic;                         /**< Magic number for validation */
  uint32_t version;                       /**< Version number */
  size_t total_size;                      /**< Total shared memory size */
  uint32_t max_nodes;                     /**< Maximum number of nodes */
  uint32_t num_active_nodes;              /**< Number of active nodes */
  uint32_t max_workers;                   /**< Maximum number of workers */
  uint32_t num_active_workers;            /**< Number of active workers */
  time_t last_update;                     /**< Last update timestamp */
  pthread_mutex_t mutex;                  /**< Mutex for synchronization */
  AuroraNodeMetricEntry node_entries[AURORA_MAX_NODES];       /**< Node metric entries */
  AuroraWorkerMetricEntry worker_entries[AURORA_MAX_WORKERS]; /**< Worker metric entries */
} AuroraMetricsShmHeader;

/**
 * @brief Opaque handle for shared memory
 */
typedef struct {
  int fd;                                 /**< Shared memory file descriptor */
  void *addr;                             /**< Mapped memory address */
  size_t size;                            /**< Size of mapped region */
  char name[AURORA_MAX_NAME_LEN];         /**< Shared memory name */
  bool is_owner;                          /**< Is this the creator? */
} AuroraMetricsShmHandle;

/**
 * @brief Create a new shared memory region for Aurora metrics
 * 
 * @param name Name of the shared memory segment
 * @param max_nodes Maximum number of nodes to support
 * @param max_workers Maximum number of workers to support
 * @return Handle to shared memory, or NULL on failure
 */
AuroraMetricsShmHandle *aurora_metrics_shm_create(const char *name, size_t max_nodes, size_t max_workers);

/**
 * @brief Connect to an existing shared memory region
 * 
 * @param name Name of the shared memory segment
 * @return Handle to shared memory, or NULL on failure
 */
AuroraMetricsShmHandle *aurora_metrics_shm_connect(const char *name);

/**
 * @brief Write a metric value for a specific node
 * 
 * @param handle Shared memory handle
 * @param node_name Name of the node
 * @param node_type Type of the node
 * @param metric_id Metric identifier
 * @param value Metric value
 * @return 0 on success, -1 on failure
 */
int aurora_metrics_shm_write_node_metric(AuroraMetricsShmHandle *handle,
                                          const char *node_name,
                                          AuroraNodeType node_type,
                                          AuroraMetricId metric_id,
                                          double value);

/**
 * @brief Write worker metrics
 * 
 * @param handle Shared memory handle
 * @param entry Worker metric entry to write
 * @return 0 on success, -1 on failure
 */
int aurora_metrics_shm_write_worker_metric(AuroraMetricsShmHandle *handle,
                                            const AuroraWorkerMetricEntry *entry);

/**
 * @brief Read a metric value for a specific node
 * 
 * @param handle Shared memory handle
 * @param node_name Name of the node
 * @param metric_id Metric identifier
 * @param value Output: metric value
 * @param timestamp Output: metric timestamp
 * @return 0 on success, -1 on failure
 */
int aurora_metrics_shm_read_node_metric(AuroraMetricsShmHandle *handle,
                                         const char *node_name,
                                         AuroraMetricId metric_id,
                                         double *value,
                                         time_t *timestamp);

/**
 * @brief Read worker metrics
 * 
 * @param handle Shared memory handle
 * @param worker_id Worker ID
 * @param entry Output: worker metric entry
 * @return 0 on success, -1 on failure
 */
int aurora_metrics_shm_read_worker_metric(AuroraMetricsShmHandle *handle,
                                           uint32_t worker_id,
                                           AuroraWorkerMetricEntry *entry);

/**
 * @brief Get pointer to shared memory header (for direct access)
 * 
 * @param handle Shared memory handle
 * @return Pointer to header, or NULL on failure
 */
AuroraMetricsShmHeader *aurora_metrics_shm_get_header(AuroraMetricsShmHandle *handle);

/**
 * @brief Destroy shared memory region
 * 
 * @param handle Shared memory handle
 */
void aurora_metrics_shm_destroy(AuroraMetricsShmHandle *handle);

#ifdef __cplusplus
}
#endif
