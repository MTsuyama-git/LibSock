project(LIBUDP)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${libsock_SOURCE_DIR}/include
)

add_library(udp
    src/UdpServer.cpp
)

target_link_libraries(udp
    PUBLIC
    sock)