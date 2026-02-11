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

/*
 * Aurora eBPF Worker Monitor – stub implementation.
 *
 * The production version of this module loads BPF programs that attach
 * to the tracepoints listed in aurora_ebpf_hook_t and filters events
 * by comm name (pthread_setname_np) against the configured worker list.
 *
 * Requirements for full implementation:
 *   - libbpf >= 0.8
 *   - Kernel >= 5.10 with BPF, PERFMON capabilities
 *   - CAP_SYS_ADMIN or CAP_BPF + CAP_PERFMON
 *
 * This stub provides the same API so the rest of Aurora compiles and
 * runs without BPF support (metrics will be zero).
 */

#include "aurora_ebpf.h"
#include <string.h>
#include <stdio.h>

/* Stored worker names for stub read */
static aurora_worker_config_t stored_cfg;
static bool initialized = false;

int aurora_ebpf_init(const aurora_worker_config_t *cfg)
{
  if (!cfg || !cfg->enabled) {
    fprintf(stderr, "aurora ebpf: worker monitoring disabled\n");
    return 0;
  }

  memcpy(&stored_cfg, cfg, sizeof(stored_cfg));
  initialized = true;

  fprintf(stderr, "aurora ebpf: stub initialized with %d workers\n",
          cfg->num_workers);
  for (int i = 0; i < cfg->num_workers; i++) {
    fprintf(stderr, "  worker[%d]: %s\n", i, cfg->worker_names[i]);
  }
  fprintf(stderr, "aurora ebpf: hooks would attach to:\n");
  for (int h = 0; h < AURORA_EBPF_HOOK_COUNT; h++) {
    fprintf(stderr, "  - %s\n", aurora_ebpf_hook_str((aurora_ebpf_hook_t)h));
  }

  return 0;
}

int aurora_ebpf_read(aurora_worker_metrics_t *workers, int *num_workers)
{
  if (!initialized || !stored_cfg.enabled) {
    *num_workers = 0;
    return 0;
  }

  int n = stored_cfg.num_workers;
  if (n > AURORA_MAX_WORKERS)
    n = AURORA_MAX_WORKERS;

  for (int i = 0; i < n; i++) {
    memset(&workers[i], 0, sizeof(aurora_worker_metrics_t));
    snprintf(workers[i].name, AURORA_WORKER_NAME_LEN, "%s",
             stored_cfg.worker_names[i]);
    workers[i].active = true;
    /* All counters remain zero in stub mode */
  }

  *num_workers = n;
  return 0;
}

void aurora_ebpf_destroy(void)
{
  initialized = false;
  memset(&stored_cfg, 0, sizeof(stored_cfg));
}
