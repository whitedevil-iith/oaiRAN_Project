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
 * @file aurora_metrics_collector.h
 * @brief Metric collection with delta computation for worker metrics
 */

#pragma once

#include "aurora_metrics_shm.h"
#include "aurora_metrics_config.h"
#include "aurora_worker_metrics.h"
#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Statistical computation functions
 */

/**
 * @brief Compute mean of values
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Mean value, or 0.0 if count is 0
 */
double aurora_compute_mean(const double *values, size_t count);

/**
 * @brief Compute variance of values
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Variance, or 0.0 if count is 0 or 1
 */
double aurora_compute_variance(const double *values, size_t count);

/**
 * @brief Compute standard deviation of values
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Standard deviation, or 0.0 if count is 0 or 1
 */
double aurora_compute_std_dev(const double *values, size_t count);

/**
 * @brief Compute skewness of values
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Skewness, or 0.0 if count < 3
 */
double aurora_compute_skewness(const double *values, size_t count);

/**
 * @brief Compute kurtosis of values
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Kurtosis, or 0.0 if count < 4
 */
double aurora_compute_kurtosis(const double *values, size_t count);

/**
 * @brief Compute interquartile range (IQR) of values
 * 
 * Note: This function modifies the input array for sorting
 * 
 * @param values Array of values (will be modified)
 * @param count Number of values
 * @return IQR, or 0.0 if count < 4
 */
double aurora_compute_iqr(double *values, size_t count);

/**
 * @brief Metric collector structure with delta tracking
 */
typedef struct {
  AuroraMetricsShmHandle *shm_handle;     /**< Shared memory handle */
  AuroraCollectorConfig config;           /**< Configuration */
  bool running;                           /**< Is collector running? */
  pthread_t collection_thread;            /**< Collection thread */
  /* Previous raw values for delta computation */
  AuroraWorkerRawMetrics prev_raw;        /**< Previous raw metrics */
  bool has_prev_raw;                      /**< Do we have previous raw metrics? */
} AuroraMetricCollector;

/**
 * @brief Initialize metric collector
 * 
 * @param collector Collector structure to initialize
 * @param shm_handle Shared memory handle
 * @param config Configuration
 * @return 0 on success, -1 on failure
 */
int aurora_collector_init(AuroraMetricCollector *collector,
                          AuroraMetricsShmHandle *shm_handle,
                          const AuroraCollectorConfig *config);

/**
 * @brief Start metric collection
 * 
 * @param collector Collector structure
 * @return 0 on success, -1 on failure
 */
int aurora_collector_start(AuroraMetricCollector *collector);

/**
 * @brief Stop metric collection
 * 
 * @param collector Collector structure
 * @return 0 on success, -1 on failure
 */
int aurora_collector_stop(AuroraMetricCollector *collector);

/**
 * @brief Perform a single collection pass
 * 
 * @param collector Collector structure
 * @return 0 on success, -1 on failure
 */
int aurora_collector_collect_once(AuroraMetricCollector *collector);

#ifdef __cplusplus
}
#endif
