find_package(OpenSSL REQUIRED)

add_executable(httpServerSimple)

target_sources(
    httpServerSimple
    PRIVATE
    HttpServerSimple_Main.cpp)

target_link_libraries(
    httpServerSimple
    PUBLIC
    chatlog
    tcp
    userinfo
    util
    http
    OpenSSL::SSL
    pthread
)
