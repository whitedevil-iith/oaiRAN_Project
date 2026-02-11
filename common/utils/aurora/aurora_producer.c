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

#include "aurora.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

aurora_shm_t *aurora_producer_init(aurora_node_type_t node_type)
{
  int fd = shm_open(AURORA_SHM_PATH, O_CREAT | O_RDWR, 0666);
  if (fd < 0) {
    perror("aurora: shm_open failed");
    return NULL;
  }

  if (ftruncate(fd, sizeof(aurora_shm_t)) < 0) {
    perror("aurora: ftruncate failed");
    close(fd);
    shm_unlink(AURORA_SHM_PATH);
    return NULL;
  }

  aurora_shm_t *shm = mmap(NULL, sizeof(aurora_shm_t),
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (shm == MAP_FAILED) {
    perror("aurora: mmap failed");
    shm_unlink(AURORA_SHM_PATH);
    return NULL;
  }

  /* Initialize the structure */
  memset(shm, 0, sizeof(aurora_shm_t));
  shm->header.magic = AURORA_SHM_MAGIC;
  shm->header.version = AURORA_VERSION;
  shm->header.node_type = node_type;

  /* Set up process-shared robust mutex */
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
  pthread_mutex_init(&shm->header.mutex, &attr);
  pthread_mutexattr_destroy(&attr);

  return shm;
}

int aurora_producer_lock(aurora_shm_t *shm)
{
  int rc = pthread_mutex_lock(&shm->header.mutex);
  if (rc == EOWNERDEAD) {
    /* Previous owner crashed; make the mutex consistent */
    pthread_mutex_consistent(&shm->header.mutex);
    return 0;
  }
  return (rc == 0) ? 0 : -1;
}

int aurora_producer_unlock(aurora_shm_t *shm)
{
  return (pthread_mutex_unlock(&shm->header.mutex) == 0) ? 0 : -1;
}

int aurora_producer_write_metric(aurora_shm_t *shm, const char *name,
                                 aurora_metric_type_t type, int64_t value)
{
  uint32_t idx = shm->header.num_node_metrics;
  if (idx >= AURORA_MAX_NODE_METRICS)
    return -1;

  aurora_raw_metric_t *m = &shm->node_metrics[idx];
  snprintf(m->name, AURORA_METRIC_NAME_LEN, "%s", name);
  m->type = type;
  m->value = value;
  shm->header.num_node_metrics = idx + 1;
  return 0;
}

int aurora_producer_write_worker(aurora_shm_t *shm, const aurora_worker_metrics_t *w)
{
  uint32_t idx = shm->header.num_workers;
  if (idx >= AURORA_MAX_WORKERS)
    return -1;

  shm->workers[idx] = *w;
  shm->header.num_workers = idx + 1;
  return 0;
}

int aurora_producer_write_traffic(aurora_shm_t *shm, const char *name,
                                  uint64_t pkt_count, uint64_t byte_count,
                                  int64_t last_pkt_size)
{
  uint32_t idx = shm->header.num_traffic_metrics;
  if (idx >= AURORA_MAX_TRAFFIC_METRICS)
    return -1;

  aurora_traffic_metric_t *t = &shm->traffic[idx];
  snprintf(t->name, AURORA_METRIC_NAME_LEN, "%s", name);
  t->packet_count = pkt_count;
  t->byte_count = byte_count;
  t->last_packet_size = last_pkt_size;
  shm->header.num_traffic_metrics = idx + 1;
  return 0;
}

void aurora_producer_update_timestamp(aurora_shm_t *shm)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  shm->header.timestamp_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void aurora_producer_destroy(aurora_shm_t *shm)
{
  if (shm && shm != MAP_FAILED) {
    pthread_mutex_destroy(&shm->header.mutex);
    munmap(shm, sizeof(aurora_shm_t));
    shm_unlink(AURORA_SHM_PATH);
  }
}
