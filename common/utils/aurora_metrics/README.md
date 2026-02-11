# Aurora Metrics Service

## Overview

The Aurora Metrics Service is a high-performance metric collection and monitoring system designed for OpenAirInterface RAN deployments. It provides real-time collection, aggregation, and analysis of performance metrics from various RAN components including CU (Central Unit), DU (Distributed Unit), O-RU (Open Radio Unit), and worker processes.

**Key Features:**
- **eBPF-style direct syscall metrics** for minimal overhead
- **Delta metrics** for accurate rate computation
- **Nanosecond-precision timestamps** using `struct timespec`
- **100ms default collection interval** for high-frequency monitoring
- **CSV export** for sidecar containers and observability platforms

## Architecture

### Components

1. **Shared Memory Layer** (`aurora_metrics_shm.c/h`)
   - Shared memory implementation using POSIX shared memory
   - Process-shared pthread mutex for synchronization
   - Support for up to 64 nodes and 256 workers
   - Magic number and version validation

2. **Metrics Collector** (`aurora_metrics_collector.c/h`)
   - Background thread for periodic metric collection
   - Statistical analysis functions (mean, variance, std dev, skewness, kurtosis, IQR)
   - Configurable collection intervals
   - Integrated O-RU, E2 KPM, and worker metric collection

3. **Worker Metrics** (`aurora_worker_metrics.c/h`)
   - **eBPF-style direct syscalls** for fast metric collection
   - CPU time via `getrusage(RUSAGE_SELF, ...)` (fast path for self process)
   - Fallback to /proc filesystem for other processes
   - Memory usage tracking (via /proc/[pid]/statm)
   - Network I/O statistics (via /proc/[pid]/net/dev)
   - Filesystem I/O tracking (via /proc/[pid]/io)
   - **Raw metric collection** with delta computation in collector
   - **Nanosecond-precision timestamps** using `clock_gettime(CLOCK_MONOTONIC, ...)`

4. **O-RU Metrics** (`aurora_oru_metrics.c/h`)
   - Radio metrics from O-RU devices (actual radios, USRP, or simulated)
   - SINR (average/min/max), RSRP, PUSCH SNR, CSI
   - PRB utilization (DL/UL percentage)
   - DL/UL throughput (bytes per interval, delta)
   - HARQ/CRC error rates (DL HARQ loss rate, UL CRC loss rate)
   - Active UE count
   - Simulated mode for testing; live mode maps to NR_mac_stats_t

5. **E2 KPM Metrics** (`aurora_e2_metrics.c/h`)
   - 3GPP TS 28.522 compliant KPM measurements exposed via E2 interface
   - DRB.PdcpSduVolumeDL/UL (Megabits, delta)
   - DRB.RlcSduDelayDl (microseconds, gauge)
   - DRB.UEThpDl/Ul (kbps, gauge)
   - RRU.PrbTotDl/Ul (percent, gauge)
   - Simulated mode for testing; live mode maps to PDCP/RLC/MAC stats

6. **Configuration** (`aurora_metrics_config.c/h`)
   - Centralized configuration management
   - Default values and validation
   - Runtime configuration support
   - **Default 100ms collection interval** for high-frequency monitoring
   - Per-source enable/disable (worker, O-RU, E2 metrics)

7. **CSV Reader** (`aurora_metrics_csv_reader.c`)
   - Standalone program for exporting metrics to CSV
   - Designed for sidecar containers
   - Exports node and worker metrics to separate CSV files
   - Configurable collection interval
   - Graceful shutdown on SIGINT/SIGTERM

### Shared Memory Schema

```c
struct AuroraMetricsShmHeader {
  uint32_t magic;                      // 0x41524F52 ("AROR")
  uint32_t version;                    // Version 1
  size_t total_size;                   // Total shared memory size
  uint32_t max_nodes;                  // Maximum nodes (up to 64)
  uint32_t num_active_nodes;           // Current active nodes
  uint32_t max_workers;                // Maximum workers (up to 256)
  uint32_t num_active_workers;         // Current active workers
  struct timespec last_update;          // Last update timestamp (nanosecond precision)
  pthread_mutex_t mutex;               // Process-shared mutex
  AuroraNodeMetricEntry node_entries[AURORA_MAX_NODES];
  AuroraWorkerMetricEntry worker_entries[AURORA_MAX_WORKERS];
};
```

### Metric Types

#### Node Metrics
- **Traffic Metrics**: Backhaul TX/RX sizes and means
- **Radio Metrics**: SINR (average/min/max), CSI
- **Loss Metrics**: DL HARQ loss rate, UL CRC loss rate

#### O-RU Radio Metrics (via `aurora_oru_metrics`)

Collected from O-RU devices (actual radios like USRP, or simulated):

| Metric ID | Name | Type | Unit | Description |
|-----------|------|------|------|-------------|
| `AURORA_METRIC_SINR_AVERAGE` | SINR Average | Gauge | dB×10 | Average SINR across UEs |
| `AURORA_METRIC_SINR_MIN` | SINR Min | Gauge | dB×10 | Minimum SINR |
| `AURORA_METRIC_SINR_MAX` | SINR Max | Gauge | dB×10 | Maximum SINR |
| `AURORA_METRIC_CSI_AVERAGE` | CSI Average | Gauge | - | Channel State Information |
| `AURORA_METRIC_RSRP_AVERAGE` | RSRP Average | Gauge | dBm | Reference Signal Received Power |
| `AURORA_METRIC_PUSCH_SNR` | PUSCH SNR | Gauge | dB×10 | PUSCH Signal-to-Noise Ratio |
| `AURORA_METRIC_PRB_UTIL_DL` | PRB Util DL | Gauge | % | DL PRB utilization |
| `AURORA_METRIC_PRB_UTIL_UL` | PRB Util UL | Gauge | % | UL PRB utilization |
| `AURORA_METRIC_NUM_ACTIVE_UES` | Active UEs | Gauge | count | Number of active UEs |
| `AURORA_METRIC_DL_TOTAL_BYTES_DELTA` | DL Bytes | Delta | bytes | DL bytes this interval |
| `AURORA_METRIC_UL_TOTAL_BYTES_DELTA` | UL Bytes | Delta | bytes | UL bytes this interval |
| `AURORA_METRIC_DL_ERRORS_DELTA` | DL Errors | Delta | count | DL HARQ errors this interval |
| `AURORA_METRIC_UL_ERRORS_DELTA` | UL Errors | Delta | count | UL CRC errors this interval |
| `AURORA_METRIC_DL_HARQ_LOSS_RATE` | DL HARQ Loss | Delta | ratio | HARQ error rate (0.0-1.0) |
| `AURORA_METRIC_UL_CRC_LOSS_RATE` | UL CRC Loss | Delta | ratio | CRC error rate (0.0-1.0) |

#### E2 KPM Metrics (via `aurora_e2_metrics`, 3GPP TS 28.522)

Metrics exposed through the E2 interface following standard KPM measurement names:

| Metric ID | KPM Name | Type | Unit | Description |
|-----------|----------|------|------|-------------|
| `AURORA_METRIC_DRB_PDCP_SDU_VOL_DL` | DRB.PdcpSduVolumeDL | Delta | Mb | PDCP SDU volume downlink |
| `AURORA_METRIC_DRB_PDCP_SDU_VOL_UL` | DRB.PdcpSduVolumeUL | Delta | Mb | PDCP SDU volume uplink |
| `AURORA_METRIC_DRB_RLC_SDU_DELAY_DL` | DRB.RlcSduDelayDl | Gauge | μs | RLC buffer sojourn time |
| `AURORA_METRIC_DRB_UE_THP_DL` | DRB.UEThpDl | Gauge | kbps | UE throughput downlink |
| `AURORA_METRIC_DRB_UE_THP_UL` | DRB.UEThpUl | Gauge | kbps | UE throughput uplink |
| `AURORA_METRIC_RRU_PRB_TOT_DL` | RRU.PrbTotDl | Gauge | % | DL PRB allocation |
| `AURORA_METRIC_RRU_PRB_TOT_UL` | RRU.PrbTotUl | Gauge | % | UL PRB allocation |

#### Worker Metrics (DELTA metrics)

All worker metrics are now stored as **DELTA values** (change since last collection), not cumulative totals. This enables accurate rate computation for high-frequency monitoring.

- **CPU Time Delta**: CPU seconds consumed during the collection interval (not total)
- **Memory RSS**: Resident Set Size in bytes (gauge, not delta)
- **Memory Heap**: Heap usage in bytes (gauge, not delta)
- **Network TX Delta**: Bytes transmitted during the collection interval
- **Network RX Delta**: Bytes received during the collection interval
- **Filesystem Read Delta**: Bytes read during the collection interval
- **Filesystem Write Delta**: Bytes written during the collection interval
- **Timestamp**: Nanosecond-precision timestamp (`struct timespec`)

#### Statistical Metrics
- **Variance**: Sample variance
- **Standard Deviation**: Sample standard deviation
- **Skewness**: Measure of asymmetry
- **Kurtosis**: Measure of tail heaviness
- **IQR**: Interquartile range (robust measure of variability)

## Usage

### Creating a Metrics Producer

```c
#include "aurora_metrics_shm.h"
#include "aurora_metrics_config.h"
#include "aurora_metrics_collector.h"

int main(void) {
  // Initialize configuration
  AuroraCollectorConfig config;
  aurora_config_init_defaults(&config);
  config.collection_interval_ms = 100;  // 100ms (default)
  config.enable_node_metrics = true;
  config.enable_worker_metrics = true;
  
  // Create shared memory
  AuroraMetricsShmHandle *shm = aurora_metrics_shm_create("aurora_metrics", 16, 32);
  if (shm == NULL) {
    fprintf(stderr, "Failed to create shared memory\n");
    return 1;
  }
  
  // Write some metrics
  aurora_metrics_shm_write_node_metric(shm, "du_node_1", 
                                        AURORA_NODE_DU,
                                        AURORA_METRIC_SINR_AVERAGE,
                                        23.5);
  
  // Initialize and start collector
  AuroraMetricCollector collector;
  aurora_collector_init(&collector, shm, &config);
  aurora_collector_start(&collector);
  
  // Let it run...
  sleep(60);
  
  // Cleanup
  aurora_collector_stop(&collector);
  aurora_metrics_shm_destroy(shm);
  
  return 0;
}
```

### Using the CSV Reader (Sidecar Container)

The CSV reader is a standalone program that exports metrics to CSV files for external monitoring systems:

```bash
# Basic usage
./aurora_metrics_csv_reader aurora_metrics /tmp/metrics 500

# Command line arguments:
#   aurora_metrics  - shared memory name
#   /tmp/metrics    - output CSV file prefix
#   500             - collection interval in milliseconds
```

This creates two CSV files:
- `/tmp/metrics.nodes.csv` - Node metrics
- `/tmp/metrics.workers.csv` - Worker metrics (DELTA values)

**Worker metrics CSV format:**
```
timestamp_sec,timestamp_nsec,worker_id,worker_name,cpu_time_delta,memory_rss,memory_heap,net_tx_bytes_delta,net_rx_bytes_delta,fs_read_bytes_delta,fs_write_bytes_delta
1770414344,860474274,4121,collector_4121,0.000165,2289664,17293312,0,0,693,0
```

**Example Kubernetes sidecar deployment:**
```yaml
apiVersion: v1
kind: Pod
metadata:
  name: ran-with-metrics
spec:
  containers:
  - name: ran-workload
    image: oai-ran:latest
    volumeMounts:
    - name: metrics-shm
      mountPath: /dev/shm
  - name: metrics-exporter
    image: oai-metrics-exporter:latest
    command: ["/usr/local/bin/aurora_metrics_csv_reader"]
    args: ["aurora_metrics", "/var/log/metrics/output", "100"]
    volumeMounts:
    - name: metrics-shm
      mountPath: /dev/shm
    - name: metrics-volume
      mountPath: /var/log/metrics
  volumes:
  - name: metrics-shm
    emptyDir:
      medium: Memory
  - name: metrics-volume
    persistentVolumeClaim:
      claimName: metrics-pvc
```

### Creating a Metrics Reader (Container)

```c
#include "aurora_metrics_shm.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
  // Connect to existing shared memory
  AuroraMetricsShmHandle *shm = aurora_metrics_shm_connect("aurora_metrics");
  if (shm == NULL) {
    fprintf(stderr, "Failed to connect to shared memory\n");
    return 1;
  }
  
  // Get header for direct access
  AuroraMetricsShmHeader *header = aurora_metrics_shm_get_header(shm);
  
  while (1) {
    // Lock mutex for reading
    pthread_mutex_lock(&header->mutex);
    
    printf("=== Aurora Metrics Snapshot ===\n");
    printf("Active Nodes: %u\n", header->num_active_nodes);
    printf("Active Workers: %u\n", header->num_active_workers);
    printf("Last Update: %ld\n", header->last_update);
    
    // Read node metrics
    for (uint32_t i = 0; i < header->max_nodes; i++) {
      if (header->node_entries[i].active) {
        printf("\nNode: %s (type %d)\n", 
               header->node_entries[i].node_name,
               header->node_entries[i].node_type);
        
        for (uint32_t j = 0; j < header->node_entries[i].num_metrics; j++) {
          AuroraMetricValue *m = &header->node_entries[i].metrics[j];
          printf("  Metric %d: %.2f (at %ld)\n", 
                 m->metric_id, m->value, m->timestamp);
        }
      }
    }
    
    // Read worker metrics
    for (uint32_t i = 0; i < header->max_workers; i++) {
      if (header->worker_entries[i].worker_id != 0) {
        AuroraWorkerMetricEntry *w = &header->worker_entries[i];
        printf("\nWorker %u (%s):\n", w->worker_id, w->worker_name);
        printf("  CPU delta: %.6fs (last interval)\n", w->cpu_time_delta);
        printf("  RSS: %.0f bytes, Heap: %.0f bytes\n", 
               w->memory_rss, w->memory_heap);
        printf("  Net TX delta: %.0f, RX delta: %.0f\n", 
               w->net_tx_bytes_delta, w->net_rx_bytes_delta);
        printf("  Timestamp: %ld.%09ld\n",
               w->timestamp.tv_sec, w->timestamp.tv_nsec);
      }
    }
    
    pthread_mutex_unlock(&header->mutex);
    
    sleep(5);  // Poll every 5 seconds
  }
  
  // Cleanup
  aurora_metrics_shm_destroy(shm);
  return 0;
}
```

### Using Statistical Functions

```c
#include "aurora_metrics_collector.h"

void analyze_metrics(void) {
  double samples[] = {10.5, 12.3, 11.8, 13.2, 10.9, 14.1, 11.5};
  size_t count = sizeof(samples) / sizeof(samples[0]);
  
  double mean = aurora_compute_mean(samples, count);
  double variance = aurora_compute_variance(samples, count);
  double std_dev = aurora_compute_std_dev(samples, count);
  double skewness = aurora_compute_skewness(samples, count);
  double kurtosis = aurora_compute_kurtosis(samples, count);
  
  printf("Mean: %.2f\n", mean);
  printf("Variance: %.2f\n", variance);
  printf("Std Dev: %.2f\n", std_dev);
  printf("Skewness: %.4f\n", skewness);
  printf("Kurtosis: %.4f\n", kurtosis);
  
  // Note: aurora_compute_iqr modifies the array
  double samples_copy[7];
  memcpy(samples_copy, samples, sizeof(samples));
  double iqr = aurora_compute_iqr(samples_copy, count);
  printf("IQR: %.2f\n", iqr);
}
```

## Configuration Guide

### Default Configuration

```c
AuroraCollectorConfig default_config = {
  .shm_name = "aurora_metrics",
  .shm_max_nodes = 16,
  .shm_max_workers = 32,
  .collection_interval_ms = 100,  // 100ms for high-frequency monitoring
  .enable_node_metrics = true,
  .enable_worker_metrics = true,
  .enable_statistical_metrics = false
};
```

### Customizing Configuration

```c
AuroraCollectorConfig config;
aurora_config_init_defaults(&config);

// Customize
strcpy(config.shm_name, "my_metrics");
config.shm_max_nodes = 32;
config.shm_max_workers = 64;
config.collection_interval_ms = 50;  // 50ms for ultra-high-frequency

// Validate
if (aurora_config_validate(&config) != 0) {
  fprintf(stderr, "Invalid configuration\n");
  return 1;
}
```

## Worker Metrics Details

### Collection Architecture

The Aurora metrics system uses a **two-phase collection approach** for worker metrics:

1. **Raw Collection Phase**: The `aurora_worker_collect_raw()` function collects monotonic (cumulative) raw values:
   - Total CPU time since process start
   - Total network bytes transmitted/received
   - Total filesystem bytes read/written
   - Current memory usage (RSS, heap)

2. **Delta Computation Phase**: The collector computes deltas by subtracting previous raw values:
   ```c
   cpu_time_delta = current_raw.cpu_time_total - prev_raw.cpu_time_total;
   ```

This approach ensures **accurate rate computation** even with high-frequency collection (100ms default).

### CPU Time

**Fast Path (eBPF-style):**
- Uses `getrusage(RUSAGE_SELF, ...)` for self process
- Direct syscall, no file parsing
- ~10x faster than /proc parsing
- Returns microsecond-precision user and system time

**Fallback Path:**
- Read from `/proc/[pid]/stat` fields 14 (utime) and 15 (stime) for other processes
- Converted from clock ticks to seconds using `sysconf(_SC_CLK_TCK)`
- Returns total CPU time (user + system)

```c
int aurora_worker_read_cpu_fast(pid_t pid, double *cpu_time);
int aurora_worker_read_cpu_proc(pid_t pid, double *cpu_time);
```

### Memory
- **RSS**: Read from `/proc/[pid]/statm` field 2, converted from pages to bytes
- **Heap**: Read from `/proc/[pid]/statm` field 6 (data segment), approximates heap usage
- Page size obtained via `sysconf(_SC_PAGESIZE)`

### Network
- Read from `/proc/[pid]/net/dev`
- Sums TX and RX bytes across all network interfaces
- Excludes loopback traffic

### Filesystem I/O
- Read from `/proc/[pid]/io` (requires elevated permissions)
- `rchar`: Bytes read (includes cache)
- `wchar`: Bytes written (includes cache)
- Returns zeros if file is not accessible

### Timestamps

All metrics use **nanosecond-precision timestamps**:
- `struct timespec` with `tv_sec` and `tv_nsec` fields
- Collected via `clock_gettime(CLOCK_MONOTONIC, ...)` for raw metrics
- Monotonic clock ensures timestamps never go backwards
- Precise timing for high-frequency (100ms) collection

### Delta Computation

The collector maintains previous raw values and computes deltas:
```c
// First collection - no delta available
if (!collector->has_prev_raw) {
  entry.cpu_time_delta = 0.0;  // Skip first sample
}
// Subsequent collections
else {
  entry.cpu_time_delta = current_raw.cpu_time_total - 
                         collector->prev_raw.cpu_time_total;
}
```

Delta values are protected against counter resets (negative values → 0).

## Build Instructions

### Building with CMake

```bash
cd /path/to/oaiRAN_Project
mkdir -p build && cd build
cmake .. -DENABLE_TESTS=ON
make aurora_metrics
make aurora_metrics_csv_reader
```

This builds:
- `libaurora_metrics.a` - Static library
- `aurora_metrics_csv_reader` - CSV export tool

### Building Tests

```bash
cd build
make test_aurora_metrics
./test_aurora_metrics
```

### Standalone Build

```bash
cd common/utils/aurora_metrics

# Build library
gcc -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
  -c aurora_metrics_shm.c aurora_metrics_collector.c \
     aurora_worker_metrics.c aurora_metrics_config.c -I.

# Build CSV reader
gcc -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
  -I. aurora_metrics_shm.c aurora_metrics_collector.c aurora_worker_metrics.c \
  aurora_metrics_config.c aurora_metrics_csv_reader.c \
  -o aurora_metrics_csv_reader -lpthread -lrt -lm

# Build tests
gcc -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
  -I. aurora_metrics_shm.c aurora_metrics_collector.c aurora_worker_metrics.c \
  aurora_metrics_config.c tests/test_aurora_metrics.c \
  -o test_aurora_metrics -lpthread -lrt -lm
```

## Testing

The test suite (`tests/test_aurora_metrics.c`) includes:

1. **Statistical Functions Test**: Validates all statistical computations
2. **Configuration Test**: Tests default values (including 100ms interval) and validation
3. **Shared Memory Test**: Tests create/connect/read/write/destroy cycle with nanosecond timestamps
4. **Worker Metrics Test**: Tests both fast (getrusage) and proc paths, raw metric collection
5. **Collector Test**: Tests delta computation and high-frequency collection

Run tests:
```bash
./test_aurora_metrics
```

Expected output:
```
=== Aurora Metrics Test Suite ===

Testing statistical functions...
  Mean test passed: 3.00
  [...]
Statistical functions tests passed!

Testing configuration...
  Default config initialized
  Config validation passed
Configuration tests passed!

Testing shared memory...
  [...]
  Node metric read: 25.50 at 1770414344.860474274
  Worker metric read: test_worker, CPU delta: 0.15
  [...]
Shared memory tests passed!

Testing worker metrics...
  CPU time (fast): 0.0017 seconds
  CPU time (proc): 0.0017 seconds
  [...]
  Collected raw metrics for PID 1234
  Raw CPU total: 0.0018, RSS: 2166784, timestamp: 429.100255129
Worker metrics tests passed!

Testing collector with delta computation...
  [...]
  Last worker entry:
    CPU delta: 0.000144 seconds
    Memory RSS: 2428928 bytes
    Net TX delta: 0 bytes
    Timestamp: 429.400731079
Collector tests passed!

=== All tests passed! ===
```

## Performance Considerations

1. **Shared Memory**: Uses `mmap` for zero-copy access
2. **Mutex**: Process-shared mutex for safe concurrent access
3. **Collection Thread**: Background thread with `clock_nanosleep()` for precise timing
4. **eBPF-style Collection**: 
   - `getrusage()` for self-process CPU (~10x faster than /proc)
   - Direct syscalls minimize overhead
   - Suitable for 100ms (and even 50ms) collection intervals
5. **Delta Computation**: O(1) subtraction, computed in collector not reader
6. **Statistical Functions**: O(n) complexity for most functions, O(n log n) for IQR (sorting)

## Thread Safety

- All shared memory access is protected by a process-shared mutex
- Statistical functions are thread-safe (no shared state)
- Collector uses a separate thread for background collection
- Multiple readers can connect to the same shared memory segment

## Error Handling

- All functions return error codes (0 for success, -1 for failure)
- Errors are logged to stderr with descriptive messages
- Invalid parameters are checked and rejected
- Missing /proc files are handled gracefully

## Limitations

- Maximum 64 nodes per shared memory segment
- Maximum 256 workers per shared memory segment
- Maximum 128 metrics per node
- `/proc/[pid]/io` requires CAP_SYS_PTRACE or root access
- Network statistics are cumulative (not per-interface breakdown)
- First collection has zero deltas (no previous baseline)
- Delta computation assumes monotonically increasing counters

## Metric Semantics

### DELTA vs GAUGE Metrics

**DELTA metrics** (change during interval):
- `cpu_time_delta` - CPU seconds consumed THIS interval
- `net_tx_bytes_delta` - Bytes transmitted THIS interval
- `net_rx_bytes_delta` - Bytes received THIS interval
- `fs_read_bytes_delta` - Bytes read THIS interval
- `fs_write_bytes_delta` - Bytes written THIS interval

**GAUGE metrics** (current value):
- `memory_rss` - Current RSS memory
- `memory_heap` - Current heap usage

Example interpretation:
```
Collection interval: 100ms
cpu_time_delta: 0.015 seconds

Interpretation: The process used 15ms of CPU time during 
the 100ms collection interval (15% CPU utilization)
```

## Future Enhancements

- E2 interface integration for real RAN metrics
- O-RU fronthaul metrics collection
- Time-series storage for historical analysis
- Alert/threshold system
- Export to Prometheus/Grafana (in addition to CSV)
- Per-interface network statistics
- CPU affinity and scheduling information
- eBPF probes for kernel-level metrics
- Support for monitoring remote processes via agent architecture

## License

OpenAirInterface Public License v1.1

## Contact

For more information about the OpenAirInterface (OAI) Software Alliance:
contact@openairinterface.org
