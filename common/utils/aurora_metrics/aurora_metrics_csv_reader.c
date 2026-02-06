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
 * @file aurora_metrics_csv_reader.c
 * @brief Standalone CSV reader for Aurora metrics (for sidecar containers)
 * 
 * This program connects to Aurora metrics shared memory and exports
 * all metrics to CSV format. Designed to run in sidecar containers
 * for monitoring and observability.
 * 
 * Usage: ./aurora_metrics_csv_reader [shm_name] [output.csv] [interval_ms]
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "aurora_metrics_shm.h"
#include "aurora_metrics_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

/* Global flag for graceful shutdown */
static volatile bool g_running = true;

/**
 * @brief Signal handler for SIGINT/SIGTERM
 */
static void signal_handler(int sig)
{
  (void)sig;
  g_running = false;
}

/**
 * @brief Print usage information
 */
static void print_usage(const char *prog_name)
{
  fprintf(stderr, "Usage: %s [shm_name] [output.csv] [interval_ms]\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Arguments:\n");
  fprintf(stderr, "  shm_name     - Name of Aurora shared memory segment (default: aurora_metrics)\n");
  fprintf(stderr, "  output.csv   - Output CSV file path (default: aurora_metrics.csv)\n");
  fprintf(stderr, "  interval_ms  - Collection interval in milliseconds (default: 1000)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Example:\n");
  fprintf(stderr, "  %s aurora_metrics /tmp/metrics.csv 500\n", prog_name);
  fprintf(stderr, "\n");
}

/**
 * @brief Write CSV headers for node metrics
 */
static int write_node_metrics_header(FILE *fp)
{
  fprintf(fp, "timestamp_sec,timestamp_nsec,node_name,node_type,metric_id,metric_value\n");
  return 0;
}

/**
 * @brief Write CSV headers for worker metrics
 */
static int write_worker_metrics_header(FILE *fp)
{
  fprintf(fp, "timestamp_sec,timestamp_nsec,worker_id,worker_name,cpu_time_delta,memory_rss,memory_heap,net_tx_bytes_delta,net_rx_bytes_delta,fs_read_bytes_delta,fs_write_bytes_delta\n");
  return 0;
}

/**
 * @brief Export node metrics to CSV
 */
static int export_node_metrics(FILE *fp, AuroraMetricsShmHandle *handle)
{
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(handle);
  if (header == NULL) {
    fprintf(stderr, "Failed to get shared memory header\n");
    return -1;
  }
  
  /* Lock mutex for reading */
  pthread_mutex_lock(&header->mutex);
  
  /* Iterate through all active nodes */
  for (uint32_t i = 0; i < header->max_nodes; i++) {
    if (!header->node_entries[i].active) {
      continue;
    }
    
    AuroraNodeMetricEntry *node = &header->node_entries[i];
    
    /* Write each metric for this node */
    for (uint32_t j = 0; j < node->num_metrics; j++) {
      AuroraMetricValue *metric = &node->metrics[j];
      
      fprintf(fp, "%ld,%ld,%s,%d,%d,%.6f\n",
              metric->timestamp.tv_sec,
              metric->timestamp.tv_nsec,
              node->node_name,
              node->node_type,
              metric->metric_id,
              metric->value);
    }
  }
  
  pthread_mutex_unlock(&header->mutex);
  return 0;
}

/**
 * @brief Export worker metrics to CSV
 */
static int export_worker_metrics(FILE *fp, AuroraMetricsShmHandle *handle)
{
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(handle);
  if (header == NULL) {
    fprintf(stderr, "Failed to get shared memory header\n");
    return -1;
  }
  
  /* Lock mutex for reading */
  pthread_mutex_lock(&header->mutex);
  
  /* Iterate through all active workers */
  for (uint32_t i = 0; i < header->max_workers; i++) {
    AuroraWorkerMetricEntry *worker = &header->worker_entries[i];
    
    /* Skip inactive entries (worker_id == 0) */
    if (worker->worker_id == 0) {
      continue;
    }
    
    fprintf(fp, "%ld,%ld,%u,%s,%.6f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
            worker->timestamp.tv_sec,
            worker->timestamp.tv_nsec,
            worker->worker_id,
            worker->worker_name,
            worker->cpu_time_delta,
            worker->memory_rss,
            worker->memory_heap,
            worker->net_tx_bytes_delta,
            worker->net_rx_bytes_delta,
            worker->fs_read_bytes_delta,
            worker->fs_write_bytes_delta);
  }
  
  pthread_mutex_unlock(&header->mutex);
  return 0;
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[])
{
  /* Parse command line arguments */
  const char *shm_name = "aurora_metrics";
  const char *output_path = "aurora_metrics.csv";
  uint32_t interval_ms = 1000;
  
  if (argc > 1) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    shm_name = argv[1];
  }
  
  if (argc > 2) {
    output_path = argv[2];
  }
  
  if (argc > 3) {
    interval_ms = (uint32_t)atoi(argv[3]);
    if (interval_ms == 0) {
      fprintf(stderr, "Invalid interval_ms: %s\n", argv[3]);
      print_usage(argv[0]);
      return 1;
    }
  }
  
  fprintf(stderr, "Aurora Metrics CSV Reader\n");
  fprintf(stderr, "  Shared memory: %s\n", shm_name);
  fprintf(stderr, "  Output file: %s\n", output_path);
  fprintf(stderr, "  Interval: %u ms\n", interval_ms);
  fprintf(stderr, "\n");
  
  /* Setup signal handlers */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  
  /* Connect to shared memory */
  fprintf(stderr, "Connecting to shared memory '%s'...\n", shm_name);
  AuroraMetricsShmHandle *handle = aurora_metrics_shm_connect(shm_name);
  if (handle == NULL) {
    fprintf(stderr, "Failed to connect to shared memory '%s'\n", shm_name);
    fprintf(stderr, "Make sure the Aurora metrics collector is running.\n");
    return 1;
  }
  fprintf(stderr, "Connected successfully.\n");
  
  /* Open output files */
  char node_output[512];
  char worker_output[512];
  snprintf(node_output, sizeof(node_output), "%s.nodes.csv", output_path);
  snprintf(worker_output, sizeof(worker_output), "%s.workers.csv", output_path);
  
  FILE *node_fp = fopen(node_output, "w");
  if (node_fp == NULL) {
    fprintf(stderr, "Failed to open node output file '%s': %s\n", 
            node_output, strerror(errno));
    aurora_metrics_shm_destroy(handle);
    return 1;
  }
  
  FILE *worker_fp = fopen(worker_output, "w");
  if (worker_fp == NULL) {
    fprintf(stderr, "Failed to open worker output file '%s': %s\n", 
            worker_output, strerror(errno));
    fclose(node_fp);
    aurora_metrics_shm_destroy(handle);
    return 1;
  }
  
  /* Write CSV headers */
  write_node_metrics_header(node_fp);
  write_worker_metrics_header(worker_fp);
  fflush(node_fp);
  fflush(worker_fp);
  
  fprintf(stderr, "Writing metrics to:\n");
  fprintf(stderr, "  Node metrics: %s\n", node_output);
  fprintf(stderr, "  Worker metrics: %s\n", worker_output);
  fprintf(stderr, "\nPress Ctrl+C to stop.\n\n");
  
  /* Main collection loop */
  uint64_t iteration = 0;
  while (g_running) {
    /* Export metrics */
    export_node_metrics(node_fp, handle);
    export_worker_metrics(worker_fp, handle);
    
    /* Flush to ensure data is written */
    fflush(node_fp);
    fflush(worker_fp);
    
    iteration++;
    if (iteration % 10 == 0) {
      fprintf(stderr, "\rCollected %lu iterations...", 
              (unsigned long)iteration);
      fflush(stderr);
    }
    
    /* Sleep for configured interval */
    usleep(interval_ms * 1000);
  }
  
  fprintf(stderr, "\n\nShutting down gracefully...\n");
  
  /* Cleanup */
  fclose(node_fp);
  fclose(worker_fp);
  aurora_metrics_shm_destroy(handle);
  
  fprintf(stderr, "Metrics written to:\n");
  fprintf(stderr, "  Node metrics: %s\n", node_output);
  fprintf(stderr, "  Worker metrics: %s\n", worker_output);
  fprintf(stderr, "Total iterations: %lu\n", (unsigned long)iteration);
  
  return 0;
}
