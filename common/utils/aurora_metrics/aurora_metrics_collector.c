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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "aurora_metrics_collector.h"
#include "aurora_worker_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

/* Statistical computation functions */

double aurora_compute_mean(const double *values, size_t count)
{
  if (values == NULL || count == 0) {
    return 0.0;
  }
  
  double sum = 0.0;
  for (size_t i = 0; i < count; i++) {
    sum += values[i];
  }
  return sum / (double)count;
}

double aurora_compute_variance(const double *values, size_t count)
{
  if (values == NULL || count <= 1) {
    return 0.0;
  }
  
  double mean = aurora_compute_mean(values, count);
  double sum_squared_diff = 0.0;
  
  for (size_t i = 0; i < count; i++) {
    double diff = values[i] - mean;
    sum_squared_diff += diff * diff;
  }
  
  return sum_squared_diff / (double)(count - 1);
}

double aurora_compute_std_dev(const double *values, size_t count)
{
  if (values == NULL || count <= 1) {
    return 0.0;
  }
  
  return sqrt(aurora_compute_variance(values, count));
}

double aurora_compute_skewness(const double *values, size_t count)
{
  if (values == NULL || count < 3) {
    return 0.0;
  }
  
  double mean = aurora_compute_mean(values, count);
  double std_dev = aurora_compute_std_dev(values, count);
  
  if (std_dev == 0.0) {
    return 0.0;
  }
  
  double sum_cubed = 0.0;
  for (size_t i = 0; i < count; i++) {
    double normalized = (values[i] - mean) / std_dev;
    sum_cubed += normalized * normalized * normalized;
  }
  
  return sum_cubed / (double)count;
}

double aurora_compute_kurtosis(const double *values, size_t count)
{
  if (values == NULL || count < 4) {
    return 0.0;
  }
  
  double mean = aurora_compute_mean(values, count);
  double std_dev = aurora_compute_std_dev(values, count);
  
  if (std_dev == 0.0) {
    return 0.0;
  }
  
  double sum_fourth = 0.0;
  for (size_t i = 0; i < count; i++) {
    double normalized = (values[i] - mean) / std_dev;
    double squared = normalized * normalized;
    sum_fourth += squared * squared;
  }
  
  /* Excess kurtosis (subtract 3 for normal distribution) */
  return (sum_fourth / (double)count) - 3.0;
}

/* Comparison function for qsort */
static int compare_doubles(const void *a, const void *b)
{
  double da = *(const double *)a;
  double db = *(const double *)b;
  if (da < db) return -1;
  if (da > db) return 1;
  return 0;
}

double aurora_compute_iqr(double *values, size_t count)
{
  if (values == NULL || count < 4) {
    return 0.0;
  }
  
  /* Sort array */
  qsort(values, count, sizeof(double), compare_doubles);
  
  /* Calculate Q1 and Q3 positions */
  size_t q1_pos = count / 4;
  size_t q3_pos = (3 * count) / 4;
  
  /* Linear interpolation for quartiles */
  double q1 = values[q1_pos];
  double q3 = values[q3_pos];
  
  return q3 - q1;
}

/* Collector functions */

/**
 * @brief Collection thread function
 */
static void *collection_thread_func(void *arg)
{
  AuroraMetricCollector *collector = (AuroraMetricCollector *)arg;
  struct timespec next_time;
  
  /* Get current time */
  clock_gettime(CLOCK_MONOTONIC, &next_time);
  
  while (collector->running) {
    /* Collect metrics */
    aurora_collector_collect_once(collector);
    
    /* Calculate next collection time for precise intervals */
    next_time.tv_sec += collector->config.collection_interval_ms / 1000;
    next_time.tv_nsec += (collector->config.collection_interval_ms % 1000) * 1000000L;
    
    /* Handle nanosecond overflow */
    if (next_time.tv_nsec >= 1000000000L) {
      next_time.tv_sec += 1;
      next_time.tv_nsec -= 1000000000L;
    }
    
    /* Sleep until next collection time */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
  }
  
  return NULL;
}

int aurora_collector_init(AuroraMetricCollector *collector,
                          AuroraMetricsShmHandle *shm_handle,
                          const AuroraCollectorConfig *config)
{
  if (collector == NULL || shm_handle == NULL || config == NULL) {
    fprintf(stderr, "Invalid parameters for collector_init\n");
    return -1;
  }
  
  if (aurora_config_validate(config) != 0) {
    fprintf(stderr, "Invalid configuration\n");
    return -1;
  }
  
  collector->shm_handle = shm_handle;
  memcpy(&collector->config, config, sizeof(AuroraCollectorConfig));
  collector->running = false;
  collector->has_prev_raw = false;
  memset(&collector->prev_raw, 0, sizeof(AuroraWorkerRawMetrics));
  
  return 0;
}

int aurora_collector_start(AuroraMetricCollector *collector)
{
  if (collector == NULL) {
    fprintf(stderr, "Collector is NULL\n");
    return -1;
  }
  
  if (collector->running) {
    fprintf(stderr, "Collector is already running\n");
    return -1;
  }
  
  collector->running = true;
  
  int ret = pthread_create(&collector->collection_thread, NULL, 
                          collection_thread_func, collector);
  if (ret != 0) {
    fprintf(stderr, "Failed to create collection thread: %s\n", strerror(ret));
    collector->running = false;
    return -1;
  }
  
  return 0;
}

int aurora_collector_stop(AuroraMetricCollector *collector)
{
  if (collector == NULL) {
    fprintf(stderr, "Collector is NULL\n");
    return -1;
  }
  
  if (!collector->running) {
    return 0;
  }
  
  collector->running = false;
  
  int ret = pthread_join(collector->collection_thread, NULL);
  if (ret != 0) {
    fprintf(stderr, "Failed to join collection thread: %s\n", strerror(ret));
    return -1;
  }
  
  return 0;
}

int aurora_collector_collect_once(AuroraMetricCollector *collector)
{
  if (collector == NULL) {
    fprintf(stderr, "Collector is NULL\n");
    return -1;
  }
  
  /* Collect worker metrics if enabled */
  if (collector->config.enable_worker_metrics) {
    /* Collect metrics for self process as an example */
    pid_t self_pid = getpid();
    
    /* Collect raw monotonic metrics */
    AuroraWorkerRawMetrics current_raw;
    memset(&current_raw, 0, sizeof(current_raw));
    
    if (aurora_worker_collect_raw(self_pid, &current_raw) == 0) {
      /* Compute deltas if we have previous values */
      AuroraWorkerMetricEntry entry;
      memset(&entry, 0, sizeof(entry));
      entry.worker_id = (uint32_t)self_pid;
      snprintf(entry.worker_name, AURORA_MAX_NAME_LEN, "collector_%d", self_pid);
      entry.timestamp = current_raw.timestamp;
      
      if (collector->has_prev_raw) {
        /* Compute deltas for monotonic counters */
        entry.cpu_time_delta = current_raw.cpu_time_total - 
                               collector->prev_raw.cpu_time_total;
        entry.net_tx_bytes_delta = current_raw.net_tx_bytes_total - 
                                   collector->prev_raw.net_tx_bytes_total;
        entry.net_rx_bytes_delta = current_raw.net_rx_bytes_total - 
                                   collector->prev_raw.net_rx_bytes_total;
        entry.fs_read_bytes_delta = current_raw.fs_read_bytes_total - 
                                    collector->prev_raw.fs_read_bytes_total;
        entry.fs_write_bytes_delta = current_raw.fs_write_bytes_total - 
                                     collector->prev_raw.fs_write_bytes_total;
        
        /* Ensure deltas are non-negative (handle counter resets) */
        if (entry.cpu_time_delta < 0.0) entry.cpu_time_delta = 0.0;
        if (entry.net_tx_bytes_delta < 0.0) entry.net_tx_bytes_delta = 0.0;
        if (entry.net_rx_bytes_delta < 0.0) entry.net_rx_bytes_delta = 0.0;
        if (entry.fs_read_bytes_delta < 0.0) entry.fs_read_bytes_delta = 0.0;
        if (entry.fs_write_bytes_delta < 0.0) entry.fs_write_bytes_delta = 0.0;
      } else {
        /* First collection - no deltas available */
        entry.cpu_time_delta = 0.0;
        entry.net_tx_bytes_delta = 0.0;
        entry.net_rx_bytes_delta = 0.0;
        entry.fs_read_bytes_delta = 0.0;
        entry.fs_write_bytes_delta = 0.0;
      }
      
      /* Copy gauge values directly (not deltas) */
      entry.memory_rss = current_raw.memory_rss;
      entry.memory_heap = current_raw.memory_heap;
      
      /* Write delta metrics to shared memory */
      aurora_metrics_shm_write_worker_metric(collector->shm_handle, &entry);
      
      /* Save current as previous for next iteration */
      memcpy(&collector->prev_raw, &current_raw, sizeof(AuroraWorkerRawMetrics));
      collector->has_prev_raw = true;
    }
  }
  
  /* Additional collection logic would go here */
  /* For example, collecting from E2 interfaces, O-RU metrics, etc. */
  
  return 0;
}
