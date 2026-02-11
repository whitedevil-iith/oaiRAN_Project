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

#include "aurora_config.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

aurora_node_type_t aurora_parse_node_type(const char *type_str)
{
  if (!type_str)
    return AURORA_NODE_CU;
  if (strcasecmp(type_str, "CU") == 0)
    return AURORA_NODE_CU;
  if (strcasecmp(type_str, "DU") == 0)
    return AURORA_NODE_DU;
  if (strcasecmp(type_str, "RU") == 0)
    return AURORA_NODE_RU;
  if (strcasecmp(type_str, "MONOLITHIC") == 0)
    return AURORA_NODE_MONOLITHIC;
  return AURORA_NODE_CU;
}

static void add_worker(aurora_config_t *cfg, const char *name)
{
  if (cfg->worker_monitoring.num_workers >= AURORA_MAX_CONFIG_WORKERS)
    return;
  int idx = cfg->worker_monitoring.num_workers;
  snprintf(cfg->worker_monitoring.worker_names[idx], AURORA_WORKER_NAME_LEN, "%s", name);
  cfg->worker_monitoring.num_workers++;
}

void aurora_config_init_defaults(aurora_config_t *cfg, aurora_node_type_t node_type)
{
  memset(cfg, 0, sizeof(*cfg));
  cfg->node_type = node_type;
  cfg->worker_monitoring.enabled = true;

  switch (node_type) {
    case AURORA_NODE_CU:
      add_worker(cfg, "oai:gnb-pdcp");
      add_worker(cfg, "oai:gnb-sdap");
      add_worker(cfg, "oai:gnb-f1c");
      add_worker(cfg, "oai:gnb-f1u");
      break;

    case AURORA_NODE_DU:
      add_worker(cfg, "oai:gnb-L1-rx");
      add_worker(cfg, "oai:gnb-L1-tx");
      add_worker(cfg, "oai:gnb-prach");
      add_worker(cfg, "oai:gnb-prach-br");
      break;

    case AURORA_NODE_MONOLITHIC:
      /* CU workers */
      add_worker(cfg, "oai:gnb-pdcp");
      add_worker(cfg, "oai:gnb-sdap");
      add_worker(cfg, "oai:gnb-f1c");
      add_worker(cfg, "oai:gnb-f1u");
      /* DU workers */
      add_worker(cfg, "oai:gnb-L1-rx");
      add_worker(cfg, "oai:gnb-L1-tx");
      add_worker(cfg, "oai:gnb-prach");
      add_worker(cfg, "oai:gnb-prach-br");
      break;

    case AURORA_NODE_RU:
      /* O-RU has no default named workers */
      break;

    default:
      break;
  }
}
