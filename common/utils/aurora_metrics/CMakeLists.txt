add_library(aurora_metrics 
  aurora_metrics_shm.c
  aurora_metrics_collector.c
  aurora_worker_metrics.c
  aurora_metrics_config.c
)
target_include_directories(aurora_metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aurora_metrics PRIVATE pthread rt)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
