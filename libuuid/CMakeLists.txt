project(libuuid)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(uuid
STATIC
src/UUID.cpp
)