project(libsock)
include_directories(${PROJECT_SOURCE_DIR}/include ${libsock_INCLUDE_DIR})

add_library(sock
STATIC
src/BindConfig.cpp
src/SocketConfig.cpp
src/ASock.cpp
)