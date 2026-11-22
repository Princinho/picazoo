qt_add_executable(picazoo-bench picazoo-bench.cpp)
set_target_properties(picazoo-bench PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(picazoo-bench PRIVATE picazoo_core)
