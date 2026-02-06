# Aurora Metrics Service

## Overview

The Aurora Metrics Service is a high-performance metric collection and monitoring system designed for OpenAirInterface RAN deployments. It provides real-time collection, aggregation, and analysis of performance metrics from various RAN components including CU (Central Unit), DU (Distributed Unit), O-RU (Open Radio Unit), and worker processes.

## Architecture

### Components

1. **Shared Memory Layer** (`aurora_metrics_shm.c/h`)
   - Lock-free shared memory implementation using POSIX shared memory
   - Process-shared pthread mutex for synchronization
   - Support for up to 64 nodes and 256 workers
   - Magic number and version validation

2. **Metrics Collector** (`aurora_metrics_collector.c/h`)
   - Background thread for periodic metric collection
   - Statistical analysis functions (mean, variance, std dev, skewness, kurtosis, IQR)
   - Configurable collection intervals
   - Extensible collection framework

3. **Worker Metrics** (`aurora_worker_metrics.c/h`)
   - Linux /proc filesystem-based metrics collection
   - CPU time monitoring (via /proc/[pid]/stat)
   - Memory usage tracking (via /proc/[pid]/statm)
   - Network I/O statistics (via /proc/[pid]/net/dev)
   - Filesystem I/O tracking (via /proc/[pid]/io)

4. **Configuration** (`aurora_metrics_config.c/h`)
   - Centralized configuration management
   - Default values and validation
   - Runtime configuration support

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
  time_t last_update;                  // Last update timestamp
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

#### Worker Metrics
- **CPU**: User + system time in seconds
- **Memory**: RSS (Resident Set Size) and heap usage in bytes
- **Network**: TX/RX bytes across all interfaces
- **Filesystem**: Read/write bytes

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
  config.collection_interval_ms = 1000;  // 1 second
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
        printf("  CPU: %.2fs, RSS: %.0f bytes\n", 
               w->cpu_time, w->memory_rss);
        printf("  Net TX: %.0f, RX: %.0f\n", 
               w->net_tx_bytes, w->net_rx_bytes);
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
  .collection_interval_ms = 1000,
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
config.collection_interval_ms = 500;  // 500ms

// Validate
if (aurora_config_validate(&config) != 0) {
  fprintf(stderr, "Invalid configuration\n");
  return 1;
}
```

## Worker Metrics Details

### CPU Time
- Read from `/proc/[pid]/stat` fields 14 (utime) and 15 (stime)
- Converted from clock ticks to seconds using `sysconf(_SC_CLK_TCK)`
- Returns total CPU time (user + system)

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

## Build Instructions

### Building the Module

The Aurora metrics module is built as part of the OAI build system:

```bash
cd /path/to/oaiRAN_Project
mkdir -p build && cd build
cmake .. -DENABLE_TESTS=ON
make aurora_metrics
```

### Building Tests

```bash
cd build
make test_aurora_metrics
./test_aurora_metrics
```

### Standalone Build

```bash
cd common/utils/aurora_metrics
gcc -Wall -Wextra -o libaurorametrics.o \
  aurora_metrics_shm.c \
  aurora_metrics_collector.c \
  aurora_worker_metrics.c \
  aurora_metrics_config.c \
  -lpthread -lrt -lm -I.
```

## Testing

The test suite (`tests/test_aurora_metrics.c`) includes:

1. **Statistical Functions Test**: Validates all statistical computations
2. **Configuration Test**: Tests default values and validation
3. **Shared Memory Test**: Tests create/connect/read/write/destroy cycle
4. **Worker Metrics Test**: Tests /proc filesystem reading
5. **Collector Test**: Tests background collection thread

Run tests:
```bash
./test_aurora_metrics
```

Expected output:
```
=== Aurora Metrics Test Suite ===

Testing statistical functions...
  Mean test passed: 3.00
  Variance test passed: 2.50
  Std dev test passed: 1.58
  Skewness test passed: 0.0000
  Kurtosis test passed: -1.2000
  IQR test passed: 4.00
  Edge case tests passed
Statistical functions tests passed!

Testing configuration...
  Default config initialized
  Config validation passed
Configuration tests passed!

[... more output ...]

=== All tests passed! ===
```

## Performance Considerations

1. **Shared Memory**: Uses `mmap` for zero-copy access
2. **Mutex**: Process-shared mutex for safe concurrent access
3. **Collection Thread**: Background thread prevents blocking main application
4. **/proc Access**: Minimal overhead, reads only necessary files
5. **Statistical Functions**: O(n) complexity for most functions, O(n log n) for IQR (sorting)

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

## Future Enhancements

- E2 interface integration for real RAN metrics
- O-RU fronthaul metrics collection
- Time-series storage for historical analysis
- Alert/threshold system
- Export to Prometheus/Grafana
- Per-interface network statistics
- CPU affinity and scheduling information

## License

OpenAirInterface Public License v1.1

## Contact

For more information about the OpenAirInterface (OAI) Software Alliance:
contact@openairinterface.org
