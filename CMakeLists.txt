project(sock_sample)
cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -D__DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/thirdparty/include
    ${PROJECT_SOURCE_DIR}/libsock/include
    ${PROJECT_SOURCE_DIR}/libtcp/include
    ${PROJECT_SOURCE_DIR}/libudp/include
    ${PROJECT_SOURCE_DIR}/libutil/include
    ${PROJECT_SOURCE_DIR}/libhttp/include
    ${PROJECT_SOURCE_DIR}/libapp/include
    /opt/homebrew/Cellar/openssl@3/3.0.7/include
)
project(tcpudp)
add_subdirectory(libutil)
add_subdirectory(libsock)
add_subdirectory(libudp)
add_subdirectory(libtcp)
add_subdirectory(libhttp)
add_subdirectory(libapp)
add_subdirectory(test)