project(LIBAPP)

include_directories(
    ${PROJECT_SOURCE_DIR}/include 
    ${PROJECT_SOURCE_DIR}/thirdparty/include 
    ./include
    )

add_library(chatlog
    src/ChatLog.cpp)

add_library(userinfo
    src/UserInfo.cpp
    )

target_link_libraries(userinfo PUBLIC
${OPENSSL_LIBRARIES}
)

target_include_directories(userinfo PUBLIC
${OPENSSL_INCLUDE_DIRS}
)