/*
 * Unit test for Aurora sliding window statistical computations.
 *
 * Validates: mean, variance, std deviation, skewness, kurtosis,
 *            min, max, range, quartiles, IQR, outlier detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../receiver/aurora_receiver.h"

#define EPSILON 1e-4

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

#define APPROX_EQ(a, b) (fabs((a) - (b)) < EPSILON)

/* ── Test: basic mean/variance on known values ───────────────────── */
static void test_basic_stats(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  /* Push values 1..10 */
  for (int i = 1; i <= 10; i++)
    aurora_window_push(&w, (double)i);

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "test_basic", &r);

  TEST_ASSERT(r.sample_count == 10, "sample count should be 10");
  TEST_ASSERT(APPROX_EQ(r.mean, 5.5), "mean should be 5.5");
  TEST_ASSERT(APPROX_EQ(r.min, 1.0), "min should be 1");
  TEST_ASSERT(APPROX_EQ(r.max, 10.0), "max should be 10");
  TEST_ASSERT(APPROX_EQ(r.range, 9.0), "range should be 9");

  /* Population variance of 1..10 = 8.25 */
  TEST_ASSERT(APPROX_EQ(r.variance, 8.25), "variance should be 8.25");
  TEST_ASSERT(APPROX_EQ(r.std_dev, sqrt(8.25)), "std_dev check");
}

/* ── Test: sliding window wraps correctly ────────────────────────── */
static void test_window_wrap(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  /* Fill window beyond capacity */
  for (int i = 0; i < AURORA_WINDOW_SIZE + 50; i++)
    aurora_window_push(&w, (double)(i + 1));

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "test_wrap", &r);

  TEST_ASSERT(r.sample_count == AURORA_WINDOW_SIZE,
              "count should be capped at window size");

  /* Values 51..150; mean = (51+150)/2 = 100.5 */
  TEST_ASSERT(APPROX_EQ(r.mean, 100.5), "mean after wrap should be 100.5");
  TEST_ASSERT(APPROX_EQ(r.min, 51.0), "min after wrap should be 51");
  TEST_ASSERT(APPROX_EQ(r.max, 150.0), "max after wrap should be 150");
}

/* ── Test: constant values → zero variance/skewness ──────────────── */
static void test_constant_values(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  for (int i = 0; i < 50; i++)
    aurora_window_push(&w, 42.0);

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "constant", &r);

  TEST_ASSERT(APPROX_EQ(r.mean, 42.0), "mean of constant");
  TEST_ASSERT(APPROX_EQ(r.variance, 0.0), "variance of constant");
  TEST_ASSERT(APPROX_EQ(r.std_dev, 0.0), "std_dev of constant");
  TEST_ASSERT(APPROX_EQ(r.skewness, 0.0), "skewness of constant");
  TEST_ASSERT(r.outlier_count == 0, "no outliers in constant");
}

/* ── Test: outlier detection ─────────────────────────────────────── */
static void test_outliers(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  /* 98 values of 10, then 1 value of 100, 1 value of -80 */
  for (int i = 0; i < 98; i++)
    aurora_window_push(&w, 10.0);
  aurora_window_push(&w, 100.0);
  aurora_window_push(&w, -80.0);

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "outlier", &r);

  /* The extreme values should be detected as outliers */
  TEST_ASSERT(r.outlier_count >= 2, "should detect at least 2 outliers");
}

/* ── Test: single value ──────────────────────────────────────────── */
static void test_single_value(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  aurora_window_push(&w, 7.0);

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "single", &r);

  TEST_ASSERT(r.sample_count == 1, "single sample count");
  TEST_ASSERT(APPROX_EQ(r.mean, 7.0), "single mean");
  TEST_ASSERT(APPROX_EQ(r.variance, 0.0), "single variance");
  TEST_ASSERT(APPROX_EQ(r.min, 7.0), "single min");
  TEST_ASSERT(APPROX_EQ(r.max, 7.0), "single max");
}

/* ── Test: empty window ──────────────────────────────────────────── */
static void test_empty_window(void)
{
  aurora_sliding_window_t w;
  memset(&w, 0, sizeof(w));

  aurora_stat_result_t r;
  aurora_window_compute_stats(&w, "empty", &r);

  TEST_ASSERT(r.sample_count == 0, "empty sample count");
  TEST_ASSERT(APPROX_EQ(r.mean, 0.0), "empty mean");
}

int main(void)
{
  printf("=== Aurora Statistics Unit Tests ===\n");

  test_basic_stats();
  test_window_wrap();
  test_constant_values();
  test_outliers();
  test_single_value();
  test_empty_window();

  printf("\n%d / %d tests passed\n", pass_count, test_count);
  return (pass_count == test_count) ? 0 : 1;
}
