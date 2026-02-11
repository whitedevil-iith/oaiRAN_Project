/*
 * Unit test for Aurora producer: shared memory creation,
 * metric writing, locking, and cleanup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../aurora.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST_ASSERT(cond, msg)                                                \
  do {                                                                        \
    test_count++;                                                             \
    if (!(cond)) {                                                            \
      fprintf(stderr, "FAIL [%d]: %s (line %d)\n", test_count, msg, __LINE__);\
    } else {                                                                  \
      pass_count++;                                                           \
    }                                                                         \
  } while (0)

/* ── Test: init/destroy lifecycle ────────────────────────────────── */
static void test_lifecycle(void)
{
  aurora_shm_t *shm = aurora_producer_init(AURORA_NODE_DU);
  TEST_ASSERT(shm != NULL, "producer init returns non-NULL");
  TEST_ASSERT(shm->header.magic == AURORA_SHM_MAGIC, "magic is set");
  TEST_ASSERT(shm->header.version == AURORA_VERSION, "version is set");
  TEST_ASSERT(shm->header.node_type == AURORA_NODE_DU, "node type is DU");
  TEST_ASSERT(shm->header.num_node_metrics == 0, "metrics start at 0");
  TEST_ASSERT(shm->header.num_workers == 0, "workers start at 0");
  aurora_producer_destroy(shm);
}

/* ── Test: write node metrics ────────────────────────────────────── */
static void test_write_metrics(void)
{
  aurora_shm_t *shm = aurora_producer_init(AURORA_NODE_CU);
  TEST_ASSERT(shm != NULL, "init for write test");

  TEST_ASSERT(aurora_producer_lock(shm) == 0, "lock succeeds");

  TEST_ASSERT(aurora_producer_write_metric(shm, "pdcp_tx_bytes",
                                           AURORA_METRIC_COUNTER, 12345) == 0,
              "write metric 1");
  TEST_ASSERT(aurora_producer_write_metric(shm, "pdcp_rx_bytes",
                                           AURORA_METRIC_COUNTER, 67890) == 0,
              "write metric 2");

  TEST_ASSERT(shm->header.num_node_metrics == 2, "two metrics written");
  TEST_ASSERT(shm->node_metrics[0].value == 12345, "metric 1 value");
  TEST_ASSERT(shm->node_metrics[1].value == 67890, "metric 2 value");
  TEST_ASSERT(strcmp(shm->node_metrics[0].name, "pdcp_tx_bytes") == 0,
              "metric 1 name");

  aurora_producer_update_timestamp(shm);
  TEST_ASSERT(shm->header.timestamp_ns > 0, "timestamp updated");

  TEST_ASSERT(aurora_producer_unlock(shm) == 0, "unlock succeeds");

  aurora_producer_destroy(shm);
}

/* ── Test: write worker metrics ──────────────────────────────────── */
static void test_write_workers(void)
{
  aurora_shm_t *shm = aurora_producer_init(AURORA_NODE_DU);
  TEST_ASSERT(shm != NULL, "init for worker test");

  aurora_producer_lock(shm);

  aurora_worker_metrics_t w = {0};
  snprintf(w.name, AURORA_WORKER_NAME_LEN, "oai:gnb-L1-rx");
  w.cpu_time_ns = 1000000;
  w.sched_switches = 42;
  w.active = true;

  TEST_ASSERT(aurora_producer_write_worker(shm, &w) == 0, "write worker");
  TEST_ASSERT(shm->header.num_workers == 1, "one worker written");
  TEST_ASSERT(strcmp(shm->workers[0].name, "oai:gnb-L1-rx") == 0,
              "worker name");
  TEST_ASSERT(shm->workers[0].cpu_time_ns == 1000000, "worker cpu_time");

  aurora_producer_unlock(shm);
  aurora_producer_destroy(shm);
}

/* ── Test: write traffic metrics ─────────────────────────────────── */
static void test_write_traffic(void)
{
  aurora_shm_t *shm = aurora_producer_init(AURORA_NODE_CU);
  TEST_ASSERT(shm != NULL, "init for traffic test");

  aurora_producer_lock(shm);

  TEST_ASSERT(aurora_producer_write_traffic(shm, "Bhtx_pdcp", 100, 150000, 1500) == 0,
              "write traffic");
  TEST_ASSERT(shm->header.num_traffic_metrics == 1, "one traffic metric");
  TEST_ASSERT(shm->traffic[0].packet_count == 100, "traffic pkt count");
  TEST_ASSERT(shm->traffic[0].byte_count == 150000, "traffic byte count");

  aurora_producer_unlock(shm);
  aurora_producer_destroy(shm);
}

/* ── Test: metric overflow protection ────────────────────────────── */
static void test_overflow(void)
{
  aurora_shm_t *shm = aurora_producer_init(AURORA_NODE_DU);
  TEST_ASSERT(shm != NULL, "init for overflow test");

  aurora_producer_lock(shm);

  for (int i = 0; i < AURORA_MAX_NODE_METRICS; i++) {
    char name[48];
    snprintf(name, sizeof(name), "metric_%d", i);
    TEST_ASSERT(aurora_producer_write_metric(shm, name,
                                             AURORA_METRIC_COUNTER, i) == 0,
                "write metric within limit");
  }

  /* Next write should fail */
  TEST_ASSERT(aurora_producer_write_metric(shm, "overflow",
                                           AURORA_METRIC_COUNTER, 999) == -1,
              "overflow write returns -1");

  aurora_producer_unlock(shm);
  aurora_producer_destroy(shm);
}

/* ── Test: node type strings ─────────────────────────────────────── */
static void test_node_type_strings(void)
{
  TEST_ASSERT(strcmp(aurora_node_type_str(AURORA_NODE_CU), "CU") == 0, "CU string");
  TEST_ASSERT(strcmp(aurora_node_type_str(AURORA_NODE_DU), "DU") == 0, "DU string");
  TEST_ASSERT(strcmp(aurora_node_type_str(AURORA_NODE_RU), "RU") == 0, "RU string");
  TEST_ASSERT(strcmp(aurora_node_type_str(AURORA_NODE_MONOLITHIC), "MONOLITHIC") == 0,
              "MONOLITHIC string");
}

int main(void)
{
  printf("=== Aurora Producer Unit Tests ===\n");

  test_lifecycle();
  test_write_metrics();
  test_write_workers();
  test_write_traffic();
  test_overflow();
  test_node_type_strings();

  printf("\n%d / %d tests passed\n", pass_count, test_count);
  return (pass_count == test_count) ? 0 : 1;
}
