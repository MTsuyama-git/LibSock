project(LIBTCP)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${libsock_SOURCE_DIR}/include
)

add_library(tcp
    src/TcpServer.cpp
    src/TcpClient.cpp)

add_library(udp
    src/UdpServer.cpp
)

target_link_libraries(tcp
    PUBLIC
    sock)

target_link_libraries(udp
    PUBLIC
    sock)