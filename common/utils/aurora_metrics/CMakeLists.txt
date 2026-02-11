add_library(aurora_metrics 
  aurora_metrics_shm.c
  aurora_metrics_collector.c
  aurora_worker_metrics.c
  aurora_metrics_config.c
  aurora_oru_metrics.c
  aurora_e2_metrics.c
)
target_include_directories(aurora_metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aurora_metrics PRIVATE pthread rt)

# CSV reader executable for sidecar containers
add_executable(aurora_metrics_csv_reader aurora_metrics_csv_reader.c)
target_link_libraries(aurora_metrics_csv_reader aurora_metrics pthread rt)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
