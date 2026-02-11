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

#include "aurora_receiver.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Receiver Init ───────────────────────────────────────────────── */

int aurora_receiver_init(aurora_receiver_t *rx, const char *csv_path)
{
  memset(rx, 0, sizeof(*rx));

  int fd = shm_open(AURORA_SHM_PATH, O_RDWR, 0666);
  if (fd < 0) {
    perror("aurora receiver: shm_open failed");
    return -1;
  }

  rx->shm = mmap(NULL, sizeof(aurora_shm_t),
                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (rx->shm == MAP_FAILED) {
    rx->shm = NULL;
    perror("aurora receiver: mmap failed");
    return -1;
  }

  if (rx->shm->header.magic != AURORA_SHM_MAGIC) {
    fprintf(stderr, "aurora receiver: bad magic in shared memory\n");
    munmap(rx->shm, sizeof(aurora_shm_t));
    rx->shm = NULL;
    return -1;
  }

  if (csv_path) {
    rx->csv_file = fopen(csv_path, "w");
    if (!rx->csv_file) {
      perror("aurora receiver: cannot open CSV");
      /* non-fatal: continue without CSV */
    }
  }

  return 0;
}

/* ── Snapshot ─────────────────────────────────────────────────────── */

int aurora_receiver_snapshot(aurora_receiver_t *rx)
{
  if (!rx->shm)
    return -1;

  int rc = pthread_mutex_lock(&rx->shm->header.mutex);
  if (rc == EOWNERDEAD)
    pthread_mutex_consistent(&rx->shm->header.mutex);
  else if (rc != 0)
    return -1;

  memcpy(&rx->snapshot, rx->shm, sizeof(aurora_shm_t));
  pthread_mutex_unlock(&rx->shm->header.mutex);
  return 0;
}

/* ── Sliding Window ──────────────────────────────────────────────── */

void aurora_window_push(aurora_sliding_window_t *w, double value)
{
  if (w->count >= AURORA_WINDOW_SIZE) {
    /* Remove oldest value from running sums */
    double old = w->values[w->head];
    w->sum -= old;
    w->sum_sq -= old * old;
  }

  w->values[w->head] = value;
  w->sum += value;
  w->sum_sq += value * value;
  w->head = (w->head + 1) % AURORA_WINDOW_SIZE;
  if (w->count < AURORA_WINDOW_SIZE)
    w->count++;
}

/* ── Comparison for qsort ────────────────────────────────────────── */

static int cmp_double(const void *a, const void *b)
{
  double da = *(const double *)a;
  double db = *(const double *)b;
  if (da < db) return -1;
  if (da > db) return 1;
  return 0;
}

/* ── Compute Statistics ──────────────────────────────────────────── */

void aurora_window_compute_stats(const aurora_sliding_window_t *w,
                                 const char *name,
                                 aurora_stat_result_t *r)
{
  memset(r, 0, sizeof(*r));
  snprintf(r->name, AURORA_METRIC_NAME_LEN, "%s", name);
  r->sample_count = w->count;

  if (w->count == 0)
    return;

  int n = w->count;

  /* Copy and sort for percentile computations */
  double sorted[AURORA_WINDOW_SIZE];
  /* Reconstruct logical order from circular buffer */
  if (n < AURORA_WINDOW_SIZE) {
    memcpy(sorted, w->values, n * sizeof(double));
  } else {
    /* full window: oldest is at head, wrap around */
    int tail = n - w->head;
    memcpy(sorted, &w->values[w->head], tail * sizeof(double));
    memcpy(&sorted[tail], w->values, w->head * sizeof(double));
  }
  qsort(sorted, n, sizeof(double), cmp_double);

  /* Min, Max, Range */
  r->min = sorted[0];
  r->max = sorted[n - 1];
  r->range = r->max - r->min;

  /* Mean */
  r->mean = w->sum / n;

  /* Variance (population) */
  r->variance = (w->sum_sq / n) - (r->mean * r->mean);
  if (r->variance < 0.0)
    r->variance = 0.0;

  /* Std deviation */
  r->std_dev = sqrt(r->variance);

  /* Quartiles */
  if (n >= 4) {
    int q1_idx = n / 4;
    int q3_idx = (3 * n) / 4;
    r->q1 = sorted[q1_idx];
    r->q3 = sorted[q3_idx];
    r->median = sorted[n / 2];
    r->iqr = r->q3 - r->q1;
  } else {
    r->q1 = r->min;
    r->q3 = r->max;
    r->median = sorted[n / 2];
    r->iqr = r->range;
  }

  /* Skewness and Kurtosis (population-based) */
  if (r->std_dev > 1e-12) {
    double sum3 = 0.0, sum4 = 0.0;
    for (int i = 0; i < n; i++) {
      double d = (sorted[i] - r->mean) / r->std_dev;
      double d2 = d * d;
      sum3 += d2 * d;
      sum4 += d2 * d2;
    }
    r->skewness = sum3 / n;
    r->kurtosis = (sum4 / n) - 3.0; /* excess kurtosis */
  }

  /* Outliers (beyond mean ± 2*std) */
  double lo = r->mean - 2.0 * r->std_dev;
  double hi = r->mean + 2.0 * r->std_dev;
  r->outlier_count = 0;
  for (int i = 0; i < n; i++) {
    if (sorted[i] < lo || sorted[i] > hi)
      r->outlier_count++;
  }
}

/* ── Process Snapshot ────────────────────────────────────────────── */

void aurora_receiver_process(aurora_receiver_t *rx,
                             aurora_stat_result_t *results,
                             int *num_results)
{
  int total = 0;

  /* Node metrics */
  uint32_t nm = rx->snapshot.header.num_node_metrics;
  if (nm > AURORA_MAX_NODE_METRICS)
    nm = AURORA_MAX_NODE_METRICS;

  for (uint32_t i = 0; i < nm; i++) {
    aurora_window_push(&rx->node_windows[i],
                       (double)rx->snapshot.node_metrics[i].value);
    aurora_window_compute_stats(&rx->node_windows[i],
                                rx->snapshot.node_metrics[i].name,
                                &results[total]);
    total++;
  }

  /* Traffic metrics */
  uint32_t tm = rx->snapshot.header.num_traffic_metrics;
  if (tm > AURORA_MAX_TRAFFIC_METRICS)
    tm = AURORA_MAX_TRAFFIC_METRICS;

  for (uint32_t i = 0; i < tm; i++) {
    aurora_window_push(&rx->traffic_windows[i],
                       (double)rx->snapshot.traffic[i].last_packet_size);
    aurora_window_compute_stats(&rx->traffic_windows[i],
                                rx->snapshot.traffic[i].name,
                                &results[total]);

    /* Compute loss ratio from packet/byte counters if available */
    if (rx->snapshot.traffic[i].packet_count > 0) {
      results[total].loss_ratio =
          1.0 - ((double)rx->snapshot.traffic[i].byte_count /
                 ((double)rx->snapshot.traffic[i].packet_count *
                  (double)rx->snapshot.traffic[i].last_packet_size));
      if (results[total].loss_ratio < 0.0)
        results[total].loss_ratio = 0.0;
    }
    total++;
  }

  *num_results = total;
}

/* ── Stale Detection ─────────────────────────────────────────────── */

bool aurora_receiver_is_stale(aurora_receiver_t *rx)
{
  uint64_t ts = rx->snapshot.header.timestamp_ns;
  if (ts == rx->last_seen_timestamp_ns) {
    rx->stale_count++;
    return rx->stale_count > 3;
  }
  rx->last_seen_timestamp_ns = ts;
  rx->stale_count = 0;
  return false;
}

/* ── CSV Export ───────────────────────────────────────────────────── */

void aurora_receiver_write_csv(aurora_receiver_t *rx,
                               const aurora_stat_result_t *results,
                               int count)
{
  if (!rx->csv_file || count == 0)
    return;

  if (!rx->csv_header_written) {
    fprintf(rx->csv_file,
            "timestamp_ns,node_type,metric_name,samples,"
            "mean,variance,std_dev,skewness,kurtosis,"
            "min,max,range,q1,median,q3,iqr,"
            "outlier_count,loss_ratio\n");
    rx->csv_header_written = true;
  }

  const char *node_str = aurora_node_type_str(rx->snapshot.header.node_type);

  for (int i = 0; i < count; i++) {
    const aurora_stat_result_t *s = &results[i];
    fprintf(rx->csv_file,
            "%lu,%s,%s,%d,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%d,%.6f\n",
            (unsigned long)rx->snapshot.header.timestamp_ns,
            node_str, s->name, s->sample_count,
            s->mean, s->variance, s->std_dev, s->skewness, s->kurtosis,
            s->min, s->max, s->range, s->q1, s->median, s->q3, s->iqr,
            s->outlier_count, s->loss_ratio);
  }
  fflush(rx->csv_file);
}

/* ── Destroy ─────────────────────────────────────────────────────── */

void aurora_receiver_destroy(aurora_receiver_t *rx)
{
  if (rx->shm && rx->shm != MAP_FAILED)
    munmap(rx->shm, sizeof(aurora_shm_t));
  rx->shm = NULL;

  if (rx->csv_file) {
    fclose(rx->csv_file);
    rx->csv_file = NULL;
  }
}
