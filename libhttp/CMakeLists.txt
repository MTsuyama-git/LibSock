project(LIBHTTP)

include_directories(
    ./include
    )

add_library(http
    src/HttpHeader.cpp)
