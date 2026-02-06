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

#include "aurora_metrics_shm.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * @brief Initialize mutex with process-shared attributes
 */
static int init_process_shared_mutex(pthread_mutex_t *mutex)
{
  pthread_mutexattr_t attr;
  int ret = pthread_mutexattr_init(&attr);
  if (ret != 0) {
    fprintf(stderr, "Failed to initialize mutex attributes: %s\n", strerror(ret));
    return -1;
  }
  
  ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    fprintf(stderr, "Failed to set mutex process-shared: %s\n", strerror(ret));
    pthread_mutexattr_destroy(&attr);
    return -1;
  }
  
  ret = pthread_mutex_init(mutex, &attr);
  if (ret != 0) {
    fprintf(stderr, "Failed to initialize mutex: %s\n", strerror(ret));
    pthread_mutexattr_destroy(&attr);
    return -1;
  }
  
  pthread_mutexattr_destroy(&attr);
  return 0;
}

AuroraMetricsShmHandle *aurora_metrics_shm_create(const char *name, size_t max_nodes, size_t max_workers)
{
  if (name == NULL || max_nodes == 0 || max_nodes > AURORA_MAX_NODES || 
      max_workers == 0 || max_workers > AURORA_MAX_WORKERS) {
    fprintf(stderr, "Invalid parameters for shared memory creation\n");
    return NULL;
  }
  
  AuroraMetricsShmHandle *handle = calloc(1, sizeof(AuroraMetricsShmHandle));
  if (handle == NULL) {
    fprintf(stderr, "Failed to allocate handle: %s\n", strerror(errno));
    return NULL;
  }
  
  snprintf(handle->name, AURORA_MAX_NAME_LEN, "%s", name);
  handle->size = sizeof(AuroraMetricsShmHeader);
  handle->is_owner = true;
  
  /* Create shared memory object */
  handle->fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0666);
  if (handle->fd == -1) {
    fprintf(stderr, "Failed to create shared memory '%s': %s\n", name, strerror(errno));
    free(handle);
    return NULL;
  }
  
  /* Set size */
  if (ftruncate(handle->fd, handle->size) == -1) {
    fprintf(stderr, "Failed to set shared memory size: %s\n", strerror(errno));
    close(handle->fd);
    shm_unlink(name);
    free(handle);
    return NULL;
  }
  
  /* Map memory */
  handle->addr = mmap(NULL, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
  if (handle->addr == MAP_FAILED) {
    fprintf(stderr, "Failed to map shared memory: %s\n", strerror(errno));
    close(handle->fd);
    shm_unlink(name);
    free(handle);
    return NULL;
  }
  
  /* Initialize header */
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  memset(header, 0, sizeof(AuroraMetricsShmHeader));
  header->magic = AURORA_SHM_MAGIC;
  header->version = AURORA_SHM_VERSION;
  header->total_size = handle->size;
  header->max_nodes = max_nodes;
  header->max_workers = max_workers;
  header->num_active_nodes = 0;
  header->num_active_workers = 0;
  header->last_update = time(NULL);
  
  if (init_process_shared_mutex(&header->mutex) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    munmap(handle->addr, handle->size);
    close(handle->fd);
    shm_unlink(name);
    free(handle);
    return NULL;
  }
  
  return handle;
}

AuroraMetricsShmHandle *aurora_metrics_shm_connect(const char *name)
{
  if (name == NULL) {
    fprintf(stderr, "Invalid name for shared memory connection\n");
    return NULL;
  }
  
  AuroraMetricsShmHandle *handle = calloc(1, sizeof(AuroraMetricsShmHandle));
  if (handle == NULL) {
    fprintf(stderr, "Failed to allocate handle: %s\n", strerror(errno));
    return NULL;
  }
  
  snprintf(handle->name, AURORA_MAX_NAME_LEN, "%s", name);
  handle->is_owner = false;
  
  /* Open existing shared memory object */
  handle->fd = shm_open(name, O_RDWR, 0666);
  if (handle->fd == -1) {
    fprintf(stderr, "Failed to open shared memory '%s': %s\n", name, strerror(errno));
    free(handle);
    return NULL;
  }
  
  /* Get size */
  struct stat st;
  if (fstat(handle->fd, &st) == -1) {
    fprintf(stderr, "Failed to stat shared memory: %s\n", strerror(errno));
    close(handle->fd);
    free(handle);
    return NULL;
  }
  handle->size = st.st_size;
  
  /* Map memory */
  handle->addr = mmap(NULL, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
  if (handle->addr == MAP_FAILED) {
    fprintf(stderr, "Failed to map shared memory: %s\n", strerror(errno));
    close(handle->fd);
    free(handle);
    return NULL;
  }
  
  /* Validate header */
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  if (header->magic != AURORA_SHM_MAGIC) {
    fprintf(stderr, "Invalid magic number in shared memory\n");
    munmap(handle->addr, handle->size);
    close(handle->fd);
    free(handle);
    return NULL;
  }
  
  if (header->version != AURORA_SHM_VERSION) {
    fprintf(stderr, "Version mismatch: expected %d, got %d\n", AURORA_SHM_VERSION, header->version);
    munmap(handle->addr, handle->size);
    close(handle->fd);
    free(handle);
    return NULL;
  }
  
  return handle;
}

int aurora_metrics_shm_write_node_metric(AuroraMetricsShmHandle *handle,
                                          const char *node_name,
                                          AuroraNodeType node_type,
                                          AuroraMetricId metric_id,
                                          double value)
{
  if (handle == NULL || node_name == NULL) {
    fprintf(stderr, "Invalid parameters for write_node_metric\n");
    return -1;
  }
  
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  
  pthread_mutex_lock(&header->mutex);
  
  /* Find or create node entry */
  AuroraNodeMetricEntry *entry = NULL;
  for (uint32_t i = 0; i < header->max_nodes; i++) {
    if (header->node_entries[i].active && 
        strcmp(header->node_entries[i].node_name, node_name) == 0) {
      entry = &header->node_entries[i];
      break;
    }
  }
  
  /* Create new entry if not found */
  if (entry == NULL) {
    for (uint32_t i = 0; i < header->max_nodes; i++) {
      if (!header->node_entries[i].active) {
        entry = &header->node_entries[i];
        entry->active = true;
        entry->node_type = node_type;
        strncpy(entry->node_name, node_name, AURORA_MAX_NAME_LEN - 1);
        entry->node_name[AURORA_MAX_NAME_LEN - 1] = '\0';
        entry->num_metrics = 0;
        header->num_active_nodes++;
        break;
      }
    }
  }
  
  if (entry == NULL) {
    fprintf(stderr, "No space for new node entry\n");
    pthread_mutex_unlock(&header->mutex);
    return -1;
  }
  
  /* Find or add metric */
  bool found = false;
  for (uint32_t i = 0; i < entry->num_metrics; i++) {
    if (entry->metrics[i].metric_id == metric_id) {
      entry->metrics[i].value = value;
      entry->metrics[i].timestamp = time(NULL);
      found = true;
      break;
    }
  }
  
  if (!found) {
    if (entry->num_metrics >= AURORA_MAX_METRICS_PER_NODE) {
      fprintf(stderr, "No space for new metric in node entry\n");
      pthread_mutex_unlock(&header->mutex);
      return -1;
    }
    entry->metrics[entry->num_metrics].metric_id = metric_id;
    entry->metrics[entry->num_metrics].value = value;
    entry->metrics[entry->num_metrics].timestamp = time(NULL);
    entry->num_metrics++;
  }
  
  header->last_update = time(NULL);
  pthread_mutex_unlock(&header->mutex);
  
  return 0;
}

int aurora_metrics_shm_write_worker_metric(AuroraMetricsShmHandle *handle,
                                            const AuroraWorkerMetricEntry *entry)
{
  if (handle == NULL || entry == NULL) {
    fprintf(stderr, "Invalid parameters for write_worker_metric\n");
    return -1;
  }
  
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  
  pthread_mutex_lock(&header->mutex);
  
  /* Find or create worker entry */
  AuroraWorkerMetricEntry *worker_entry = NULL;
  for (uint32_t i = 0; i < header->max_workers; i++) {
    if (header->worker_entries[i].worker_id == entry->worker_id) {
      worker_entry = &header->worker_entries[i];
      break;
    }
  }
  
  /* Create new entry if not found */
  if (worker_entry == NULL) {
    for (uint32_t i = 0; i < header->max_workers; i++) {
      if (header->worker_entries[i].worker_id == 0) {
        worker_entry = &header->worker_entries[i];
        header->num_active_workers++;
        break;
      }
    }
  }
  
  if (worker_entry == NULL) {
    fprintf(stderr, "No space for new worker entry\n");
    pthread_mutex_unlock(&header->mutex);
    return -1;
  }
  
  /* Copy entry data */
  memcpy(worker_entry, entry, sizeof(AuroraWorkerMetricEntry));
  header->last_update = time(NULL);
  
  pthread_mutex_unlock(&header->mutex);
  
  return 0;
}

int aurora_metrics_shm_read_node_metric(AuroraMetricsShmHandle *handle,
                                         const char *node_name,
                                         AuroraMetricId metric_id,
                                         double *value,
                                         time_t *timestamp)
{
  if (handle == NULL || node_name == NULL || value == NULL) {
    fprintf(stderr, "Invalid parameters for read_node_metric\n");
    return -1;
  }
  
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  
  pthread_mutex_lock(&header->mutex);
  
  /* Find node entry */
  for (uint32_t i = 0; i < header->max_nodes; i++) {
    if (header->node_entries[i].active && 
        strcmp(header->node_entries[i].node_name, node_name) == 0) {
      /* Find metric */
      for (uint32_t j = 0; j < header->node_entries[i].num_metrics; j++) {
        if (header->node_entries[i].metrics[j].metric_id == metric_id) {
          *value = header->node_entries[i].metrics[j].value;
          if (timestamp != NULL) {
            *timestamp = header->node_entries[i].metrics[j].timestamp;
          }
          pthread_mutex_unlock(&header->mutex);
          return 0;
        }
      }
    }
  }
  
  pthread_mutex_unlock(&header->mutex);
  return -1;
}

int aurora_metrics_shm_read_worker_metric(AuroraMetricsShmHandle *handle,
                                           uint32_t worker_id,
                                           AuroraWorkerMetricEntry *entry)
{
  if (handle == NULL || entry == NULL) {
    fprintf(stderr, "Invalid parameters for read_worker_metric\n");
    return -1;
  }
  
  AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
  
  pthread_mutex_lock(&header->mutex);
  
  /* Find worker entry */
  for (uint32_t i = 0; i < header->max_workers; i++) {
    if (header->worker_entries[i].worker_id == worker_id) {
      memcpy(entry, &header->worker_entries[i], sizeof(AuroraWorkerMetricEntry));
      pthread_mutex_unlock(&header->mutex);
      return 0;
    }
  }
  
  pthread_mutex_unlock(&header->mutex);
  return -1;
}

AuroraMetricsShmHeader *aurora_metrics_shm_get_header(AuroraMetricsShmHandle *handle)
{
  if (handle == NULL) {
    return NULL;
  }
  return (AuroraMetricsShmHeader *)handle->addr;
}

void aurora_metrics_shm_destroy(AuroraMetricsShmHandle *handle)
{
  if (handle == NULL) {
    return;
  }
  
  if (handle->addr != NULL && handle->addr != MAP_FAILED) {
    /* Destroy mutex if we're the owner */
    if (handle->is_owner) {
      AuroraMetricsShmHeader *header = (AuroraMetricsShmHeader *)handle->addr;
      pthread_mutex_destroy(&header->mutex);
    }
    
    munmap(handle->addr, handle->size);
  }
  
  if (handle->fd != -1) {
    close(handle->fd);
  }
  
  /* Unlink shared memory if we're the owner */
  if (handle->is_owner) {
    shm_unlink(handle->name);
  }
  
  free(handle);
}
