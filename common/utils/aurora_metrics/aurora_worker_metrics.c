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

#define _POSIX_C_SOURCE 200809L

#include "aurora_worker_metrics.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int aurora_worker_read_cpu(pid_t pid, double *cpu_time)
{
  if (cpu_time == NULL) {
    fprintf(stderr, "cpu_time is NULL\n");
    return -1;
  }
  
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
    return -1;
  }
  
  /* Parse /proc/[pid]/stat
   * Field 14 (utime) and 15 (stime) contain user and system CPU time in clock ticks */
  unsigned long utime = 0, stime = 0;
  int ret = fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                   &utime, &stime);
  fclose(fp);
  
  if (ret != 2) {
    fprintf(stderr, "Failed to parse CPU time from %s\n", path);
    return -1;
  }
  
  /* Convert clock ticks to seconds */
  long clock_ticks = sysconf(_SC_CLK_TCK);
  if (clock_ticks <= 0) {
    fprintf(stderr, "Invalid clock ticks: %ld\n", clock_ticks);
    return -1;
  }
  
  *cpu_time = (double)(utime + stime) / (double)clock_ticks;
  return 0;
}

int aurora_worker_read_memory(pid_t pid, double *rss_bytes, double *heap_bytes)
{
  if (rss_bytes == NULL || heap_bytes == NULL) {
    fprintf(stderr, "Output pointers are NULL\n");
    return -1;
  }
  
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/statm", pid);
  
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
    return -1;
  }
  
  /* Parse /proc/[pid]/statm
   * Field 2 (rss) contains resident set size in pages
   * Field 6 (data) contains data segment size in pages (approximation of heap) */
  unsigned long rss = 0, data = 0;
  int ret = fscanf(fp, "%*u %lu %*u %*u %*u %lu", &rss, &data);
  fclose(fp);
  
  if (ret != 2) {
    fprintf(stderr, "Failed to parse memory from %s\n", path);
    return -1;
  }
  
  /* Convert pages to bytes */
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    fprintf(stderr, "Invalid page size: %ld\n", page_size);
    return -1;
  }
  
  *rss_bytes = (double)(rss * page_size);
  *heap_bytes = (double)(data * page_size);
  return 0;
}

int aurora_worker_read_network(pid_t pid, double *tx_bytes, double *rx_bytes)
{
  if (tx_bytes == NULL || rx_bytes == NULL) {
    fprintf(stderr, "Output pointers are NULL\n");
    return -1;
  }
  
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/net/dev", pid);
  
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
    return -1;
  }
  
  /* Skip first two header lines */
  char line[512];
  if (fgets(line, sizeof(line), fp) == NULL || fgets(line, sizeof(line), fp) == NULL) {
    fprintf(stderr, "Failed to read header from %s\n", path);
    fclose(fp);
    return -1;
  }
  
  /* Sum up all interfaces */
  unsigned long long total_rx = 0, total_tx = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    char iface[32];
    unsigned long long rx, tx;
    /* Format: interface: rx_bytes ... tx_bytes ... */
    if (sscanf(line, "%31[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu",
               iface, &rx, &tx) == 3) {
      total_rx += rx;
      total_tx += tx;
    }
  }
  fclose(fp);
  
  *rx_bytes = (double)total_rx;
  *tx_bytes = (double)total_tx;
  return 0;
}

int aurora_worker_read_io(pid_t pid, double *read_bytes, double *write_bytes)
{
  if (read_bytes == NULL || write_bytes == NULL) {
    fprintf(stderr, "Output pointers are NULL\n");
    return -1;
  }
  
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/io", pid);
  
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    /* /proc/[pid]/io requires special permissions, may not be accessible */
    *read_bytes = 0.0;
    *write_bytes = 0.0;
    return 0;  /* Return success with zeros rather than failing */
  }
  
  /* Parse /proc/[pid]/io */
  char line[256];
  unsigned long long rchar = 0, wchar = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (sscanf(line, "rchar: %llu", &rchar) == 1) {
      continue;
    }
    if (sscanf(line, "wchar: %llu", &wchar) == 1) {
      continue;
    }
  }
  fclose(fp);
  
  *read_bytes = (double)rchar;
  *write_bytes = (double)wchar;
  return 0;
}

int aurora_worker_collect_all(pid_t pid, AuroraWorkerMetricEntry *entry)
{
  if (entry == NULL) {
    fprintf(stderr, "Entry is NULL\n");
    return -1;
  }
  
  /* Collect all metrics */
  if (aurora_worker_read_cpu(pid, &entry->cpu_time) != 0) {
    entry->cpu_time = 0.0;
  }
  
  if (aurora_worker_read_memory(pid, &entry->memory_rss, &entry->memory_heap) != 0) {
    entry->memory_rss = 0.0;
    entry->memory_heap = 0.0;
  }
  
  if (aurora_worker_read_network(pid, &entry->net_tx_bytes, &entry->net_rx_bytes) != 0) {
    entry->net_tx_bytes = 0.0;
    entry->net_rx_bytes = 0.0;
  }
  
  if (aurora_worker_read_io(pid, &entry->fs_read_bytes, &entry->fs_write_bytes) != 0) {
    entry->fs_read_bytes = 0.0;
    entry->fs_write_bytes = 0.0;
  }
  
  entry->timestamp = time(NULL);
  return 0;
}
