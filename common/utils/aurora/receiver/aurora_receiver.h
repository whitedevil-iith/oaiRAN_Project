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

#ifndef AURORA_RECEIVER_H
#define AURORA_RECEIVER_H

#include "../aurora.h"
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sliding window parameters ───────────────────────────────────── */
#define AURORA_WINDOW_SIZE 100

/* ── Sliding Window for a single metric ──────────────────────────── */
typedef struct {
  double values[AURORA_WINDOW_SIZE];
  int count;     /* total samples inserted (capped at window for stats) */
  int head;      /* next write position in circular buffer */
  double sum;
  double sum_sq;
} aurora_sliding_window_t;

/* ── Computed Statistics ─────────────────────────────────────────── */
typedef struct {
  char name[AURORA_METRIC_NAME_LEN];
  int sample_count;
  double mean;
  double variance;
  double std_dev;
  double skewness;
  double kurtosis;
  double min;
  double max;
  double range;
  double q1;
  double median;
  double q3;
  double iqr;
  int outlier_count;
  double loss_ratio;
} aurora_stat_result_t;

/* ── Receiver Context ────────────────────────────────────────────── */
typedef struct {
  aurora_shm_t *shm;
  aurora_shm_t snapshot;

  /* Per-node-metric sliding windows */
  aurora_sliding_window_t node_windows[AURORA_MAX_NODE_METRICS];

  /* Per-traffic-metric sliding windows */
  aurora_sliding_window_t traffic_windows[AURORA_MAX_TRAFFIC_METRICS];

  /* Stale detection */
  uint64_t last_seen_timestamp_ns;
  int stale_count;

  /* CSV output */
  FILE *csv_file;
  bool csv_header_written;
} aurora_receiver_t;

/**
 * Initialize the receiver: open and map existing shared memory.
 * @param csv_path Path for CSV output (NULL to disable CSV)
 * @return 0 on success, -1 on failure
 */
int aurora_receiver_init(aurora_receiver_t *rx, const char *csv_path);

/**
 * Take a snapshot of shared memory under the lock.
 * @return 0 on success, -1 on failure
 */
int aurora_receiver_snapshot(aurora_receiver_t *rx);

/**
 * Push a value into a sliding window.
 */
void aurora_window_push(aurora_sliding_window_t *w, double value);

/**
 * Compute full statistics from a sliding window.
 * @param name Metric name to attach to the result
 * @param result Output statistics structure
 */
void aurora_window_compute_stats(const aurora_sliding_window_t *w,
                                 const char *name,
                                 aurora_stat_result_t *result);

/**
 * Process the current snapshot: feed values into sliding windows
 * and compute statistics.
 * @param results Output array (must hold at least num_node + num_traffic entries)
 * @param num_results Output: total number of results written
 */
void aurora_receiver_process(aurora_receiver_t *rx,
                             aurora_stat_result_t *results,
                             int *num_results);

/**
 * Check if the producer is stale (timestamp not advancing).
 * @return true if stale
 */
bool aurora_receiver_is_stale(aurora_receiver_t *rx);

/**
 * Write a row of statistics to the CSV file.
 * @param results Array of stat results
 * @param count Number of results
 */
void aurora_receiver_write_csv(aurora_receiver_t *rx,
                               const aurora_stat_result_t *results,
                               int count);

/**
 * Clean up receiver resources.
 */
void aurora_receiver_destroy(aurora_receiver_t *rx);

#ifdef __cplusplus
}
#endif

#endif /* AURORA_RECEIVER_H */
