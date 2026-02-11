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

#ifndef AURORA_EBPF_H
#define AURORA_EBPF_H

#include "../aurora.h"
#include "../aurora_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * eBPF hook types that Aurora can attach to for worker monitoring.
 * These are the tracepoints and kprobes used to collect per-worker
 * resource metrics without /proc usage.
 */
typedef enum {
  AURORA_EBPF_SCHED_SWITCH = 0,
  AURORA_EBPF_SCHED_WAKEUP,
  AURORA_EBPF_KMALLOC,
  AURORA_EBPF_MMAP,
  AURORA_EBPF_UDP_SENDMSG,
  AURORA_EBPF_SCTP_SENDMSG,
  AURORA_EBPF_UDP_RECVMSG,
  AURORA_EBPF_VFS_READ,
  AURORA_EBPF_VFS_WRITE,
  AURORA_EBPF_HOOK_COUNT
} aurora_ebpf_hook_t;

static inline const char *aurora_ebpf_hook_str(aurora_ebpf_hook_t h)
{
  static const char *names[] = {
      "sched_switch",  "sched_wakeup", "kmalloc",
      "mmap",          "udp_sendmsg",  "sctp_sendmsg",
      "udp_recvmsg",   "vfs_read",     "vfs_write",
  };
  return (h < AURORA_EBPF_HOOK_COUNT) ? names[h] : "unknown";
}

/**
 * Initialize the eBPF worker monitor.
 * Loads BPF programs and attaches to the configured hooks.
 * @param cfg Configuration with worker names to filter
 * @return 0 on success, -1 on failure (e.g., missing CAP_BPF)
 *
 * NOTE: This is a stub in the initial implementation.
 *       Full eBPF loading requires libbpf and kernel support.
 */
int aurora_ebpf_init(const aurora_worker_config_t *cfg);

/**
 * Read current per-worker metrics from eBPF maps.
 * @param workers Output array (must hold AURORA_MAX_WORKERS entries)
 * @param num_workers Output: number of workers with data
 * @return 0 on success, -1 on failure
 *
 * NOTE: Stub implementation populates zeroed metrics.
 */
int aurora_ebpf_read(aurora_worker_metrics_t *workers, int *num_workers);

/**
 * Detach all eBPF programs and release resources.
 */
void aurora_ebpf_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* AURORA_EBPF_H */
