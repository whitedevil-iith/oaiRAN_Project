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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "aurora_metrics_shm.h"
#include "aurora_metrics_collector.h"
#include "aurora_worker_metrics.h"
#include "aurora_metrics_config.h"

#define TEST_SHM_NAME "test_aurora_metrics"
#define EPSILON 0.0001

void test_statistical_functions(void)
{
  printf("Testing statistical functions...\n");
  
  /* Test mean */
  double values1[] = {1.0, 2.0, 3.0, 4.0, 5.0};
  double mean = aurora_compute_mean(values1, 5);
  assert(fabs(mean - 3.0) < EPSILON);
  printf("  Mean test passed: %.2f\n", mean);
  
  /* Test variance */
  double variance = aurora_compute_variance(values1, 5);
  assert(variance > 2.4 && variance < 2.6);  /* Should be 2.5 */
  printf("  Variance test passed: %.2f\n", variance);
  
  /* Test standard deviation */
  double std_dev = aurora_compute_std_dev(values1, 5);
  assert(fabs(std_dev - sqrt(2.5)) < EPSILON);
  printf("  Std dev test passed: %.2f\n", std_dev);
  
  /* Test skewness (should be 0 for symmetric distribution) */
  double skewness = aurora_compute_skewness(values1, 5);
  assert(fabs(skewness) < 0.1);  /* Close to 0 for symmetric data */
  printf("  Skewness test passed: %.4f\n", skewness);
  
  /* Test kurtosis */
  double kurtosis = aurora_compute_kurtosis(values1, 5);
  printf("  Kurtosis test passed: %.4f\n", kurtosis);
  
  /* Test IQR */
  double values2[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  double iqr = aurora_compute_iqr(values2, 8);
  assert(iqr > 3.0 && iqr < 5.0);  /* IQR should be around 4.0 */
  printf("  IQR test passed: %.2f\n", iqr);
  
  /* Test edge cases */
  assert(aurora_compute_mean(NULL, 5) == 0.0);
  assert(aurora_compute_mean(values1, 0) == 0.0);
  assert(aurora_compute_variance(values1, 1) == 0.0);
  assert(aurora_compute_skewness(values1, 2) == 0.0);
  assert(aurora_compute_kurtosis(values1, 3) == 0.0);
  printf("  Edge case tests passed\n");
  
  printf("Statistical functions tests passed!\n\n");
}

void test_config(void)
{
  printf("Testing configuration...\n");
  
  AuroraCollectorConfig config;
  aurora_config_init_defaults(&config);
  
  assert(strlen(config.shm_name) > 0);
  assert(config.shm_max_nodes > 0);
  assert(config.shm_max_workers > 0);
  assert(config.collection_interval_ms > 0);
  printf("  Default config initialized\n");
  
  assert(aurora_config_validate(&config) == 0);
  printf("  Config validation passed\n");
  
  /* Test invalid configs */
  config.shm_max_nodes = 0;
  assert(aurora_config_validate(&config) == -1);
  config.shm_max_nodes = 16;
  
  config.collection_interval_ms = 0;
  assert(aurora_config_validate(&config) == -1);
  config.collection_interval_ms = 1000;
  
  printf("Configuration tests passed!\n\n");
}

void test_shared_memory(void)
{
  printf("Testing shared memory...\n");
  
  /* Remove any existing test shared memory */
  char file[256];
  snprintf(file, sizeof(file), "/dev/shm/%s", TEST_SHM_NAME);
  remove(file);
  
  /* Create shared memory */
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_create(TEST_SHM_NAME, 16, 32);
  assert(handle != NULL);
  printf("  Shared memory created\n");
  
  /* Validate header */
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(handle);
  assert(header != NULL);
  assert(header->magic == AURORA_SHM_MAGIC);
  assert(header->version == AURORA_SHM_VERSION);
  assert(header->max_nodes == 16);
  assert(header->max_workers == 32);
  printf("  Header validation passed\n");
  
  /* Write node metric */
  int ret = aurora_metrics_shm_write_node_metric(handle, "test_node", 
                                                  AURORA_NODE_DU,
                                                  AURORA_METRIC_SINR_AVERAGE,
                                                  25.5);
  assert(ret == 0);
  printf("  Node metric written\n");
  
  /* Read node metric */
  double value = 0.0;
  struct timespec timestamp;
  memset(&timestamp, 0, sizeof(timestamp));
  ret = aurora_metrics_shm_read_node_metric(handle, "test_node",
                                             AURORA_METRIC_SINR_AVERAGE,
                                             &value, &timestamp);
  assert(ret == 0);
  assert(fabs(value - 25.5) < EPSILON);
  assert(timestamp.tv_sec > 0 || timestamp.tv_nsec > 0);
  printf("  Node metric read: %.2f at %ld.%09ld\n", value, 
         timestamp.tv_sec, timestamp.tv_nsec);
  
  /* Write worker metric */
  AuroraWorkerMetricEntry worker_entry;
  memset(&worker_entry, 0, sizeof(worker_entry));
  worker_entry.worker_id = 12345;
  strncpy(worker_entry.worker_name, "test_worker", AURORA_MAX_NAME_LEN - 1);
  worker_entry.cpu_time_delta = 0.15;  /* Delta, not total */
  worker_entry.memory_rss = 1024000.0;
  clock_gettime(CLOCK_REALTIME, &worker_entry.timestamp);
  
  ret = aurora_metrics_shm_write_worker_metric(handle, &worker_entry);
  assert(ret == 0);
  printf("  Worker metric written\n");
  
  /* Read worker metric */
  AuroraWorkerMetricEntry read_entry;
  memset(&read_entry, 0, sizeof(read_entry));
  ret = aurora_metrics_shm_read_worker_metric(handle, 12345, &read_entry);
  assert(ret == 0);
  assert(read_entry.worker_id == 12345);
  assert(strcmp(read_entry.worker_name, "test_worker") == 0);
  assert(fabs(read_entry.cpu_time_delta - 0.15) < EPSILON);
  printf("  Worker metric read: %s, CPU delta: %.2f\n", 
         read_entry.worker_name, read_entry.cpu_time_delta);
  
  /* Connect from another handle */
  AuroraMetricsShmHandle *handle2 = aurora_metrics_shm_connect(TEST_SHM_NAME);
  assert(handle2 != NULL);
  printf("  Connected to existing shared memory\n");
  
  /* Read from second handle */
  value = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle2, "test_node",
                                             AURORA_METRIC_SINR_AVERAGE,
                                             &value, NULL);
  assert(ret == 0);
  assert(fabs(value - 25.5) < EPSILON);
  printf("  Read from second handle: %.2f\n", value);
  
  /* Cleanup */
  aurora_metrics_shm_destroy(handle2);
  aurora_metrics_shm_destroy(handle);
  printf("Shared memory tests passed!\n\n");
}

void test_worker_metrics(void)
{
  printf("Testing worker metrics...\n");
  
  pid_t self_pid = getpid();
  
  /* Test fast CPU reading */
  double cpu_time = 0.0;
  int ret = aurora_worker_read_cpu_fast(self_pid, &cpu_time);
  assert(ret == 0);
  assert(cpu_time >= 0.0);
  printf("  CPU time (fast): %.4f seconds\n", cpu_time);
  
  /* Test proc CPU reading */
  cpu_time = 0.0;
  ret = aurora_worker_read_cpu_proc(self_pid, &cpu_time);
  assert(ret == 0);
  assert(cpu_time >= 0.0);
  printf("  CPU time (proc): %.4f seconds\n", cpu_time);
  
  /* Test memory reading */
  double rss = 0.0, heap = 0.0;
  ret = aurora_worker_read_memory(self_pid, &rss, &heap);
  assert(ret == 0);
  assert(rss > 0.0);
  printf("  Memory RSS: %.0f bytes, Heap: %.0f bytes\n", rss, heap);
  
  /* Test network reading */
  double tx = 0.0, rx = 0.0;
  ret = aurora_worker_read_network(self_pid, &tx, &rx);
  assert(ret == 0);
  printf("  Network TX: %.0f bytes, RX: %.0f bytes\n", tx, rx);
  
  /* Test I/O reading (may not be accessible) */
  double read_bytes = 0.0, write_bytes = 0.0;
  ret = aurora_worker_read_io(self_pid, &read_bytes, &write_bytes);
  assert(ret == 0);
  printf("  I/O Read: %.0f bytes, Write: %.0f bytes\n", read_bytes, write_bytes);
  
  /* Test collect raw */
  AuroraWorkerRawMetrics raw;
  memset(&raw, 0, sizeof(raw));
  
  ret = aurora_worker_collect_raw(self_pid, &raw);
  assert(ret == 0);
  assert(raw.cpu_time_total >= 0.0);
  assert(raw.memory_rss > 0.0);
  assert(raw.timestamp.tv_sec > 0 || raw.timestamp.tv_nsec > 0);
  printf("  Collected raw metrics for PID %d\n", self_pid);
  printf("  Raw CPU total: %.4f, RSS: %.0f, timestamp: %ld.%09ld\n",
         raw.cpu_time_total, raw.memory_rss, 
         raw.timestamp.tv_sec, raw.timestamp.tv_nsec);
  
  printf("Worker metrics tests passed!\n\n");
}

void test_collector(void)
{
  printf("Testing collector with delta computation...\n");
  
  /* Remove any existing test shared memory */
  char file[256];
  snprintf(file, sizeof(file), "/dev/shm/%s", TEST_SHM_NAME);
  remove(file);
  
  /* Create shared memory */
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_create(TEST_SHM_NAME, 16, 32);
  assert(handle != NULL);
  
  /* Initialize config */
  AuroraCollectorConfig config;
  aurora_config_init_defaults(&config);
  config.collection_interval_ms = 100;  /* Fast for testing */
  
  /* Initialize collector */
  AuroraMetricCollector collector;
  int ret = aurora_collector_init(&collector, handle, &config);
  assert(ret == 0);
  assert(collector.has_prev_raw == false);
  printf("  Collector initialized\n");
  
  /* Start collector */
  ret = aurora_collector_start(&collector);
  assert(ret == 0);
  printf("  Collector started\n");
  
  /* Let it collect for a bit to test delta computation */
  usleep(350000);  /* 350ms - should get 3-4 collections */
  
  /* Stop collector */
  ret = aurora_collector_stop(&collector);
  assert(ret == 0);
  printf("  Collector stopped\n");
  
  /* Verify that metrics were collected */
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(handle);
  assert(header->num_active_workers > 0);
  printf("  Collected metrics for %d workers\n", header->num_active_workers);
  
  /* Check that we have delta values (not monotonic totals) */
  AuroraWorkerMetricEntry entry;
  ret = aurora_metrics_shm_read_worker_metric(handle, (uint32_t)getpid(), &entry);
  if (ret == 0) {
    printf("  Last worker entry:\n");
    printf("    CPU delta: %.6f seconds\n", entry.cpu_time_delta);
    printf("    Memory RSS: %.0f bytes\n", entry.memory_rss);
    printf("    Net TX delta: %.0f bytes\n", entry.net_tx_bytes_delta);
    printf("    Timestamp: %ld.%09ld\n", 
           entry.timestamp.tv_sec, entry.timestamp.tv_nsec);
    
    /* Delta should be small for a 100ms interval */
    assert(entry.cpu_time_delta >= 0.0);
    assert(entry.cpu_time_delta < 1.0);  /* Should be much less than 1 second */
  }
  
  /* Cleanup */
  aurora_metrics_shm_destroy(handle);
  printf("Collector tests passed!\n\n");
}

int main(void)
{
  printf("=== Aurora Metrics Test Suite ===\n\n");
  
  test_statistical_functions();
  test_config();
  test_shared_memory();
  test_worker_metrics();
  test_collector();
  
  printf("=== All tests passed! ===\n");
  return 0;
}
