cmake_minimum_required(VERSION 3.13)
project(ax_redis_auto_del)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(abseil-cpp)

add_library(ax_redis_auto_del SHARED library.cpp)
target_link_libraries(ax_redis_auto_del absl::flat_hash_map)
