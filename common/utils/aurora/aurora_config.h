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

#ifndef AURORA_CONFIG_H
#define AURORA_CONFIG_H

#include "aurora.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AURORA_MAX_CONFIG_WORKERS 32

/* ── Worker monitoring configuration ─────────────────────────────── */
typedef struct {
  bool enabled;
  int num_workers;
  char worker_names[AURORA_MAX_CONFIG_WORKERS][AURORA_WORKER_NAME_LEN];
} aurora_worker_config_t;

/* ── Full Aurora configuration ───────────────────────────────────── */
typedef struct {
  aurora_node_type_t node_type;
  aurora_worker_config_t worker_monitoring;
} aurora_config_t;

/**
 * Parse a node type string (case-insensitive) into aurora_node_type_t.
 * Recognized: "CU", "DU", "RU", "MONOLITHIC"
 * @return The corresponding node type, or AURORA_NODE_CU as default
 */
aurora_node_type_t aurora_parse_node_type(const char *type_str);

/**
 * Initialize a default configuration based on node type.
 * Populates default worker names appropriate for CU, DU, or MONOLITHIC.
 */
void aurora_config_init_defaults(aurora_config_t *cfg, aurora_node_type_t node_type);

#ifdef __cplusplus
}
#endif

#endif /* AURORA_CONFIG_H */
