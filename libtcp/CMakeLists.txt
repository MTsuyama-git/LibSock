project(LIBTCP)

include_directories(
    ${PROJECT_SOURCE_DIR}/include 
    ${libsock_SOURCE_DIR}/include
    )

add_library(tcp
src/TcpServer.cpp)

target_link_libraries(tcp
PUBLIC
sock)