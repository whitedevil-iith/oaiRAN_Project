add_library(aurora_producer aurora_producer.c aurora_config.c)
target_include_directories(aurora_producer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aurora_producer PRIVATE pthread rt)

add_library(aurora_receiver receiver/aurora_receiver.c)
target_include_directories(aurora_receiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/receiver)
target_link_libraries(aurora_receiver PRIVATE aurora_producer m pthread rt)

add_library(aurora_ebpf ebpf/aurora_ebpf.c)
target_include_directories(aurora_ebpf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ebpf)
target_link_libraries(aurora_ebpf PRIVATE aurora_producer)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
