cmake_minimum_required(VERSION 3.16)
project(vcu_platform_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(vcu_core
    src/state/vehicle_state.cpp
    src/comm/can_socket.cpp
    src/diag/dtc_manager.cpp
    src/diag/uds_server.cpp)
target_include_directories(vcu_core PUBLIC src)

add_executable(vcu src/main.cpp)
target_link_libraries(vcu PRIVATE vcu_core)