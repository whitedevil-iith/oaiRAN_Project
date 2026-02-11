/*
 * Unit test for Aurora configuration: node type parsing,
 * default worker configuration for CU/DU/MONOLITHIC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../aurora_config.h"

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

/* ── Test: node type parsing ─────────────────────────────────────── */
static void test_parse_node_type(void)
{
  TEST_ASSERT(aurora_parse_node_type("CU") == AURORA_NODE_CU, "parse CU");
  TEST_ASSERT(aurora_parse_node_type("cu") == AURORA_NODE_CU, "parse cu lowercase");
  TEST_ASSERT(aurora_parse_node_type("DU") == AURORA_NODE_DU, "parse DU");
  TEST_ASSERT(aurora_parse_node_type("du") == AURORA_NODE_DU, "parse du lowercase");
  TEST_ASSERT(aurora_parse_node_type("RU") == AURORA_NODE_RU, "parse RU");
  TEST_ASSERT(aurora_parse_node_type("MONOLITHIC") == AURORA_NODE_MONOLITHIC,
              "parse MONOLITHIC");
  TEST_ASSERT(aurora_parse_node_type(NULL) == AURORA_NODE_CU, "parse NULL defaults to CU");
  TEST_ASSERT(aurora_parse_node_type("unknown") == AURORA_NODE_CU,
              "parse unknown defaults to CU");
}

/* ── Test: CU default workers ────────────────────────────────────── */
static void test_cu_defaults(void)
{
  aurora_config_t cfg;
  aurora_config_init_defaults(&cfg, AURORA_NODE_CU);

  TEST_ASSERT(cfg.node_type == AURORA_NODE_CU, "CU node type");
  TEST_ASSERT(cfg.worker_monitoring.enabled == true, "CU monitoring enabled");
  TEST_ASSERT(cfg.worker_monitoring.num_workers == 4, "CU has 4 default workers");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[0], "oai:gnb-pdcp") == 0,
              "CU worker 0 is pdcp");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[1], "oai:gnb-sdap") == 0,
              "CU worker 1 is sdap");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[2], "oai:gnb-f1c") == 0,
              "CU worker 2 is f1c");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[3], "oai:gnb-f1u") == 0,
              "CU worker 3 is f1u");
}

/* ── Test: DU default workers ────────────────────────────────────── */
static void test_du_defaults(void)
{
  aurora_config_t cfg;
  aurora_config_init_defaults(&cfg, AURORA_NODE_DU);

  TEST_ASSERT(cfg.node_type == AURORA_NODE_DU, "DU node type");
  TEST_ASSERT(cfg.worker_monitoring.num_workers == 4, "DU has 4 default workers");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[0], "oai:gnb-L1-rx") == 0,
              "DU worker 0 is L1-rx");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[1], "oai:gnb-L1-tx") == 0,
              "DU worker 1 is L1-tx");
}

/* ── Test: MONOLITHIC default workers ────────────────────────────── */
static void test_monolithic_defaults(void)
{
  aurora_config_t cfg;
  aurora_config_init_defaults(&cfg, AURORA_NODE_MONOLITHIC);

  TEST_ASSERT(cfg.node_type == AURORA_NODE_MONOLITHIC, "MONO node type");
  TEST_ASSERT(cfg.worker_monitoring.num_workers == 8,
              "MONOLITHIC has 8 default workers (CU+DU)");
  /* First 4 are CU workers, next 4 are DU workers */
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[0], "oai:gnb-pdcp") == 0,
              "MONO worker 0 is pdcp");
  TEST_ASSERT(strcmp(cfg.worker_monitoring.worker_names[4], "oai:gnb-L1-rx") == 0,
              "MONO worker 4 is L1-rx");
}

/* ── Test: RU default workers ────────────────────────────────────── */
static void test_ru_defaults(void)
{
  aurora_config_t cfg;
  aurora_config_init_defaults(&cfg, AURORA_NODE_RU);

  TEST_ASSERT(cfg.node_type == AURORA_NODE_RU, "RU node type");
  TEST_ASSERT(cfg.worker_monitoring.num_workers == 0, "RU has 0 default workers");
}

int main(void)
{
  printf("=== Aurora Configuration Unit Tests ===\n");

  test_parse_node_type();
  test_cu_defaults();
  test_du_defaults();
  test_monolithic_defaults();
  test_ru_defaults();

  printf("\n%d / %d tests passed\n", pass_count, test_count);
  return (pass_count == test_count) ? 0 : 1;
}
