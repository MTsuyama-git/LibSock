project(sock_sample)
cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -D__DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

include_directories(
    ${PROJECT_SOURCE_DIR}/libsock/include
    ${PROJECT_SOURCE_DIR}/libtcp/include
    ${PROJECT_SOURCE_DIR}/libudp/include
)
project(tcpudp)
add_subdirectory(libsock)
add_subdirectory(libudp)
add_subdirectory(libtcp)
add_subdirectory(test)