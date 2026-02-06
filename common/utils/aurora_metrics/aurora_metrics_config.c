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

#include "aurora_metrics_config.h"
#include <string.h>
#include <stdio.h>

void aurora_config_init_defaults(AuroraCollectorConfig *config)
{
  if (config == NULL) {
    return;
  }
  
  strncpy(config->shm_name, "aurora_metrics", AURORA_MAX_NAME_LEN - 1);
  config->shm_name[AURORA_MAX_NAME_LEN - 1] = '\0';
  config->shm_max_nodes = 16;
  config->shm_max_workers = 32;
  config->collection_interval_ms = 100;  /* 100ms for high-frequency collection */
  config->enable_node_metrics = true;
  config->enable_worker_metrics = true;
  config->enable_statistical_metrics = false;
}

int aurora_config_validate(const AuroraCollectorConfig *config)
{
  if (config == NULL) {
    fprintf(stderr, "Configuration is NULL\n");
    return -1;
  }
  
  if (config->shm_name[0] == '\0') {
    fprintf(stderr, "Shared memory name is empty\n");
    return -1;
  }
  
  if (config->shm_max_nodes == 0 || config->shm_max_nodes > AURORA_MAX_NODES) {
    fprintf(stderr, "Invalid shm_max_nodes: %zu (must be 1-%d)\n", 
            config->shm_max_nodes, AURORA_MAX_NODES);
    return -1;
  }
  
  if (config->shm_max_workers == 0 || config->shm_max_workers > AURORA_MAX_WORKERS) {
    fprintf(stderr, "Invalid shm_max_workers: %zu (must be 1-%d)\n", 
            config->shm_max_workers, AURORA_MAX_WORKERS);
    return -1;
  }
  
  if (config->collection_interval_ms == 0) {
    fprintf(stderr, "Collection interval must be greater than 0\n");
    return -1;
  }
  
  return 0;
}
