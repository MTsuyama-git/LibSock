project(LIBUTIL)

include_directories(${PROJECT_SOURCE_DIR}/include ./include)

add_library(util
    src/StrUtils.cpp)
