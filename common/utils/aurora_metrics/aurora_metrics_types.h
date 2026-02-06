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

/**
 * @file aurora_metrics_types.h
 * @brief Type definitions for Aurora metrics collection service
 */

#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum constants */
#define AURORA_MAX_NODES 64
#define AURORA_MAX_METRICS_PER_NODE 128
#define AURORA_MAX_WORKERS 256
#define AURORA_MAX_NAME_LEN 64

/**
 * @brief Node type enumeration
 */
typedef enum {
  AURORA_NODE_CU = 0,     /**< Central Unit */
  AURORA_NODE_DU = 1,     /**< Distributed Unit */
  AURORA_NODE_ORU = 2,    /**< O-RAN Radio Unit */
  AURORA_NODE_WORKER = 3  /**< Worker process/thread */
} AuroraNodeType;

/**
 * @brief Metric ID enumeration
 */
typedef enum {
  /* Traffic metrics */
  AURORA_METRIC_BHTX_IN_SIZE = 0,    /**< Backhaul TX input size */
  AURORA_METRIC_BHTX_OUT_SIZE = 1,   /**< Backhaul TX output size */
  AURORA_METRIC_BHRX_IN_MEAN = 2,    /**< Backhaul RX input mean */
  AURORA_METRIC_BHRX_OUT_MEAN = 3,   /**< Backhaul RX output mean */
  
  /* Radio metrics */
  AURORA_METRIC_SINR_AVERAGE = 10,   /**< Signal-to-Interference-plus-Noise Ratio average */
  AURORA_METRIC_SINR_MIN = 11,       /**< SINR minimum */
  AURORA_METRIC_SINR_MAX = 12,       /**< SINR maximum */
  AURORA_METRIC_CSI_AVERAGE = 13,    /**< Channel State Information average */
  
  /* Loss metrics */
  AURORA_METRIC_DL_HARQ_LOSS_RATE = 20,  /**< Downlink HARQ loss rate */
  AURORA_METRIC_UL_CRC_LOSS_RATE = 21,   /**< Uplink CRC loss rate */
  
  /* Worker metrics */
  AURORA_METRIC_CPU_TIME = 30,       /**< CPU time consumed */
  AURORA_METRIC_MEMORY_RSS = 31,     /**< Resident Set Size memory */
  AURORA_METRIC_MEMORY_HEAP = 32,    /**< Heap memory usage */
  AURORA_METRIC_NET_TX_BYTES = 33,   /**< Network transmitted bytes */
  AURORA_METRIC_NET_RX_BYTES = 34,   /**< Network received bytes */
  AURORA_METRIC_FS_READ_BYTES = 35,  /**< Filesystem read bytes */
  AURORA_METRIC_FS_WRITE_BYTES = 36, /**< Filesystem write bytes */
  
  /* Statistical metrics */
  AURORA_METRIC_VARIANCE = 40,       /**< Variance */
  AURORA_METRIC_STD_DEV = 41,        /**< Standard deviation */
  AURORA_METRIC_SKEWNESS = 42,       /**< Skewness */
  AURORA_METRIC_KURTOSIS = 43,       /**< Kurtosis */
  AURORA_METRIC_IQR = 44             /**< Interquartile range */
} AuroraMetricId;

/**
 * @brief Single metric value with timestamp
 */
typedef struct {
  AuroraMetricId metric_id;  /**< Metric identifier */
  double value;              /**< Metric value */
  time_t timestamp;          /**< Time of collection */
} AuroraMetricValue;

/**
 * @brief Worker metric entry
 */
typedef struct {
  uint32_t worker_id;                  /**< Worker/process/thread ID */
  char worker_name[AURORA_MAX_NAME_LEN]; /**< Worker name */
  double cpu_time;                     /**< CPU time in seconds */
  double memory_rss;                   /**< RSS memory in bytes */
  double memory_heap;                  /**< Heap memory in bytes */
  double net_tx_bytes;                 /**< Network TX bytes */
  double net_rx_bytes;                 /**< Network RX bytes */
  double fs_read_bytes;                /**< Filesystem read bytes */
  double fs_write_bytes;               /**< Filesystem write bytes */
  time_t timestamp;                    /**< Collection timestamp */
} AuroraWorkerMetricEntry;

#ifdef __cplusplus
}
#endif
