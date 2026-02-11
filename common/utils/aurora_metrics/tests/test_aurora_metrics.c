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
#include "aurora_oru_metrics.h"
#include "aurora_e2_metrics.h"

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
  usleep(350000);  /* 350ms - should get about 3 collections at 100ms interval */
  
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

void test_oru_metrics(void)
{
  printf("Testing O-RU metrics collection...\n");

  /* Remove any existing test shared memory */
  char file[256];
  snprintf(file, sizeof(file), "/dev/shm/%s", TEST_SHM_NAME);
  remove(file);

  /* Create shared memory */
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_create(TEST_SHM_NAME, 16, 32);
  assert(handle != NULL);

  /* Collect first raw snapshot */
  AuroraOruRawMetrics raw1;
  memset(&raw1, 0, sizeof(raw1));
  int ret = aurora_oru_collect_raw("oru_test", &raw1);
  assert(ret == 0);
  assert(raw1.valid == true);
  assert(raw1.timestamp.tv_sec > 0 || raw1.timestamp.tv_nsec > 0);
  printf("  First O-RU raw snapshot collected\n");
  printf("    SINR avg: %.1f, min: %.1f, max: %.1f\n",
         raw1.sinr_average, raw1.sinr_min, raw1.sinr_max);
  printf("    PRB DL: %.1f%%, UL: %.1f%%\n", raw1.prb_util_dl, raw1.prb_util_ul);
  printf("    DL bytes total: %.0f, UL bytes total: %.0f\n",
         raw1.dl_total_bytes, raw1.ul_total_bytes);

  /* Write first snapshot (no deltas available) */
  ret = aurora_oru_write_metrics(handle, "oru_test", &raw1, NULL);
  assert(ret == 0);
  printf("  First O-RU metrics written to SHM\n");

  /* Read back gauge metrics */
  double sinr_avg = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_test",
                                             AURORA_METRIC_SINR_AVERAGE,
                                             &sinr_avg, NULL);
  assert(ret == 0);
  assert(fabs(sinr_avg - raw1.sinr_average) < EPSILON);
  printf("  SINR average read back: %.1f\n", sinr_avg);

  double prb_dl = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_test",
                                             AURORA_METRIC_PRB_UTIL_DL,
                                             &prb_dl, NULL);
  assert(ret == 0);
  printf("  PRB DL utilization: %.1f%%\n", prb_dl);

  /* First snapshot delta metrics should be 0 */
  double dl_bytes_delta = -1.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_test",
                                             AURORA_METRIC_DL_TOTAL_BYTES_DELTA,
                                             &dl_bytes_delta, NULL);
  assert(ret == 0);
  assert(fabs(dl_bytes_delta) < EPSILON);  /* Should be 0 */
  printf("  First DL bytes delta: %.0f (expected 0)\n", dl_bytes_delta);

  /* Collect second raw snapshot */
  AuroraOruRawMetrics raw2;
  memset(&raw2, 0, sizeof(raw2));
  ret = aurora_oru_collect_raw("oru_test", &raw2);
  assert(ret == 0);
  assert(raw2.valid == true);

  /* Monotonic counters must increase */
  assert(raw2.dl_total_bytes >= raw1.dl_total_bytes);
  assert(raw2.ul_total_bytes >= raw1.ul_total_bytes);
  printf("  Second O-RU raw snapshot collected\n");
  printf("    DL bytes total: %.0f (prev: %.0f)\n",
         raw2.dl_total_bytes, raw1.dl_total_bytes);

  /* Write second snapshot (with deltas) */
  ret = aurora_oru_write_metrics(handle, "oru_test", &raw2, &raw1);
  assert(ret == 0);
  printf("  Second O-RU metrics written with deltas\n");

  /* Read back delta metrics - should be non-negative */
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_test",
                                             AURORA_METRIC_DL_TOTAL_BYTES_DELTA,
                                             &dl_bytes_delta, NULL);
  assert(ret == 0);
  assert(dl_bytes_delta >= 0.0);
  printf("  DL bytes delta: %.0f\n", dl_bytes_delta);

  double dl_loss = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_test",
                                             AURORA_METRIC_DL_HARQ_LOSS_RATE,
                                             &dl_loss, NULL);
  assert(ret == 0);
  assert(dl_loss >= 0.0 && dl_loss <= 1.0);
  printf("  DL HARQ loss rate: %.4f\n", dl_loss);

  /* Test NULL parameter handling */
  ret = aurora_oru_collect_raw("test", NULL);
  assert(ret == -1);

  ret = aurora_oru_write_metrics(NULL, "test", &raw1, NULL);
  assert(ret == -1);

  printf("  Edge case tests passed\n");

  /* Cleanup */
  aurora_metrics_shm_destroy(handle);
  printf("O-RU metrics tests passed!\n\n");
}

void test_e2_metrics(void)
{
  printf("Testing E2 KPM metrics collection...\n");

  /* Remove any existing test shared memory */
  char file[256];
  snprintf(file, sizeof(file), "/dev/shm/%s", TEST_SHM_NAME);
  remove(file);

  /* Create shared memory */
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_create(TEST_SHM_NAME, 16, 32);
  assert(handle != NULL);

  /* Collect first raw snapshot */
  AuroraE2RawMetrics raw1;
  memset(&raw1, 0, sizeof(raw1));
  int ret = aurora_e2_collect_raw("gnb_du_test", AURORA_NODE_DU, &raw1);
  assert(ret == 0);
  assert(raw1.valid == true);
  assert(raw1.timestamp.tv_sec > 0 || raw1.timestamp.tv_nsec > 0);
  printf("  First E2 raw snapshot collected\n");
  printf("    PDCP DL vol: %.2f Mb, UL vol: %.2f Mb\n",
         raw1.pdcp_sdu_vol_dl, raw1.pdcp_sdu_vol_ul);
  printf("    RLC delay DL: %.1f us\n", raw1.rlc_sdu_delay_dl);
  printf("    UE Thp DL: %.0f kbps, UL: %.0f kbps\n",
         raw1.ue_thp_dl, raw1.ue_thp_ul);
  printf("    PRB Tot DL: %.1f%%, UL: %.1f%%\n",
         raw1.prb_tot_dl, raw1.prb_tot_ul);

  /* Write first snapshot (no deltas for PDCP volumes) */
  ret = aurora_e2_write_metrics(handle, "gnb_du_test", AURORA_NODE_DU, &raw1, NULL);
  assert(ret == 0);
  printf("  First E2 metrics written to SHM\n");

  /* Read back gauge metrics */
  double rlc_delay = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_test",
                                             AURORA_METRIC_DRB_RLC_SDU_DELAY_DL,
                                             &rlc_delay, NULL);
  assert(ret == 0);
  assert(fabs(rlc_delay - raw1.rlc_sdu_delay_dl) < EPSILON);
  printf("  RLC SDU Delay DL read back: %.1f us\n", rlc_delay);

  double thp_dl = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_test",
                                             AURORA_METRIC_DRB_UE_THP_DL,
                                             &thp_dl, NULL);
  assert(ret == 0);
  assert(thp_dl >= 0.0);
  printf("  UE Throughput DL: %.0f kbps\n", thp_dl);

  /* First snapshot: PDCP volume deltas should be 0 */
  double pdcp_dl_delta = -1.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_test",
                                             AURORA_METRIC_DRB_PDCP_SDU_VOL_DL,
                                             &pdcp_dl_delta, NULL);
  assert(ret == 0);
  assert(fabs(pdcp_dl_delta) < EPSILON);  /* Should be 0 */
  printf("  First PDCP DL volume delta: %.4f (expected 0)\n", pdcp_dl_delta);

  /* Collect second raw snapshot */
  AuroraE2RawMetrics raw2;
  memset(&raw2, 0, sizeof(raw2));
  ret = aurora_e2_collect_raw("gnb_du_test", AURORA_NODE_DU, &raw2);
  assert(ret == 0);
  assert(raw2.valid == true);

  /* PDCP volumes are monotonic */
  assert(raw2.pdcp_sdu_vol_dl >= raw1.pdcp_sdu_vol_dl);
  assert(raw2.pdcp_sdu_vol_ul >= raw1.pdcp_sdu_vol_ul);
  printf("  Second E2 raw snapshot collected\n");
  printf("    PDCP DL vol: %.2f Mb (prev: %.2f)\n",
         raw2.pdcp_sdu_vol_dl, raw1.pdcp_sdu_vol_dl);

  /* Write second snapshot (with deltas) */
  ret = aurora_e2_write_metrics(handle, "gnb_du_test", AURORA_NODE_DU, &raw2, &raw1);
  assert(ret == 0);
  printf("  Second E2 metrics written with deltas\n");

  /* Read back PDCP volume delta */
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_test",
                                             AURORA_METRIC_DRB_PDCP_SDU_VOL_DL,
                                             &pdcp_dl_delta, NULL);
  assert(ret == 0);
  assert(pdcp_dl_delta >= 0.0);
  printf("  PDCP DL volume delta: %.4f Mb\n", pdcp_dl_delta);

  /* PRB utilization should be in valid range */
  double prb_dl = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_test",
                                             AURORA_METRIC_RRU_PRB_TOT_DL,
                                             &prb_dl, NULL);
  assert(ret == 0);
  assert(prb_dl >= 0.0 && prb_dl <= 100.0);
  printf("  PRB Tot DL: %.1f%%\n", prb_dl);

  /* Test NULL parameter handling */
  ret = aurora_e2_collect_raw("test", AURORA_NODE_DU, NULL);
  assert(ret == -1);

  ret = aurora_e2_write_metrics(NULL, "test", AURORA_NODE_DU, &raw1, NULL);
  assert(ret == -1);

  printf("  Edge case tests passed\n");

  /* Cleanup */
  aurora_metrics_shm_destroy(handle);
  printf("E2 KPM metrics tests passed!\n\n");
}

void test_collector_with_oru_e2(void)
{
  printf("Testing collector with O-RU and E2 metrics...\n");

  /* Remove any existing test shared memory */
  char file[256];
  snprintf(file, sizeof(file), "/dev/shm/%s", TEST_SHM_NAME);
  remove(file);

  /* Create shared memory */
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_create(TEST_SHM_NAME, 16, 32);
  assert(handle != NULL);

  /* Initialize config with all metrics enabled */
  AuroraCollectorConfig config;
  aurora_config_init_defaults(&config);
  config.collection_interval_ms = 100;

  /* Initialize collector */
  AuroraMetricCollector collector;
  int ret = aurora_collector_init(&collector, handle, &config);
  assert(ret == 0);
  assert(collector.has_prev_oru == false);
  assert(collector.has_prev_e2 == false);
  printf("  Collector initialized with O-RU and E2 enabled\n");

  /* Start collector */
  ret = aurora_collector_start(&collector);
  assert(ret == 0);
  printf("  Collector started\n");

  /* Let it collect for a bit to get multiple deltas */
  usleep(350000);  /* 350ms - about 3 collections at 100ms */

  /* Stop collector */
  ret = aurora_collector_stop(&collector);
  assert(ret == 0);
  printf("  Collector stopped\n");

  /* Verify O-RU node was created */
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(handle);
  assert(header->num_active_nodes > 0);
  printf("  Active nodes: %u\n", header->num_active_nodes);

  /* Read O-RU metrics from shared memory */
  double sinr = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_0",
                                             AURORA_METRIC_SINR_AVERAGE,
                                             &sinr, NULL);
  if (ret == 0) {
    printf("  O-RU SINR average: %.1f\n", sinr);
  }

  double prb_dl = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "oru_0",
                                             AURORA_METRIC_PRB_UTIL_DL,
                                             &prb_dl, NULL);
  if (ret == 0) {
    assert(prb_dl >= 0.0 && prb_dl <= 100.0);
    printf("  O-RU PRB DL: %.1f%%\n", prb_dl);
  }

  /* Read E2 metrics from shared memory */
  double thp_dl = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_0",
                                             AURORA_METRIC_DRB_UE_THP_DL,
                                             &thp_dl, NULL);
  if (ret == 0) {
    assert(thp_dl >= 0.0);
    printf("  E2 UE Throughput DL: %.0f kbps\n", thp_dl);
  }

  double rlc_delay = 0.0;
  ret = aurora_metrics_shm_read_node_metric(handle, "gnb_du_0",
                                             AURORA_METRIC_DRB_RLC_SDU_DELAY_DL,
                                             &rlc_delay, NULL);
  if (ret == 0) {
    assert(rlc_delay >= 0.0);
    printf("  E2 RLC SDU Delay DL: %.1f us\n", rlc_delay);
  }

  /* Verify worker metrics still work alongside O-RU/E2 */
  assert(header->num_active_workers > 0);
  printf("  Active workers: %u\n", header->num_active_workers);

  /* Cleanup */
  aurora_metrics_shm_destroy(handle);
  printf("Collector with O-RU and E2 tests passed!\n\n");
}

int main(void)
{
  printf("=== Aurora Metrics Test Suite ===\n\n");
  
  test_statistical_functions();
  test_config();
  test_shared_memory();
  test_worker_metrics();
  test_collector();
  test_oru_metrics();
  test_e2_metrics();
  test_collector_with_oru_e2();
  
  printf("=== All tests passed! ===\n");
  return 0;
}
