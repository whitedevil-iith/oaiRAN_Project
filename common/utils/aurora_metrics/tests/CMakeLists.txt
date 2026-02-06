add_executable(test_aurora_metrics test_aurora_metrics.c)
target_link_libraries(test_aurora_metrics aurora_metrics pthread rt m)
add_dependencies(tests test_aurora_metrics)
add_test(NAME test_aurora_metrics COMMAND ./test_aurora_metrics)
