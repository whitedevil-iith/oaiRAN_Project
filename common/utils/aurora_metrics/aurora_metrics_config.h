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
 * @file aurora_metrics_config.h
 * @brief Configuration for Aurora metrics collector
 */

#pragma once

#include "aurora_metrics_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Collector configuration structure
 */
typedef struct {
  char shm_name[AURORA_MAX_NAME_LEN];     /**< Shared memory segment name */
  size_t shm_max_nodes;                   /**< Maximum nodes to track */
  size_t shm_max_workers;                 /**< Maximum workers to track */
  uint32_t collection_interval_ms;        /**< Collection interval in milliseconds */
  bool enable_node_metrics;               /**< Enable E2/O-RU node metrics */
  bool enable_worker_metrics;             /**< Enable worker process monitoring */
  bool enable_statistical_metrics;        /**< Enable derived statistics */
  bool enable_oru_metrics;                /**< Enable O-RU radio metrics collection */
  bool enable_e2_metrics;                 /**< Enable E2 KPM metrics collection */
} AuroraCollectorConfig;

/**
 * @brief Initialize configuration with default values
 * 
 * @param config Configuration structure to initialize
 */
void aurora_config_init_defaults(AuroraCollectorConfig *config);

/**
 * @brief Validate configuration
 * 
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int aurora_config_validate(const AuroraCollectorConfig *config);

#ifdef __cplusplus
}
#endif
