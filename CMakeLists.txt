cmake_minimum_required(VERSION 3.10)
project(Sockray CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

add_executable(sockray
    src/main.cpp
    src/port_scanner.cpp
    src/process_server.cpp
    src/tcp_handler.cpp
    src/udp_handler.cpp
)
