find_package(OpenSSL REQUIRED)

add_executable(test_tcp_server
    test_tcp_server.cpp)
target_link_libraries(test_tcp_server
    PUBLIC
    tcp
)

add_executable(websocket_sample
    websocket_sample.cpp)

add_executable(chat_app_sample
    chat_app_sample.cpp)
target_link_libraries(websocket_sample
    PUBLIC
    tcp
    util
    OpenSSL::SSL
)

target_link_libraries(chat_app_sample
    PUBLIC
    tcp
    util
    chatlog
    userinfo
    http
    OpenSSL::SSL
)

add_executable(test_udp_server
    test_udp_server.cpp)
target_link_libraries(test_udp_server
    PUBLIC
    udp
)

add_executable(test_tcp_client
    test_tcp_client.cpp)

target_link_libraries(test_tcp_client
    PUBLIC
    tcp
)

add_executable(test_b64
test_base64.cpp)

target_link_libraries(test_b64 PUBLIC util) 

add_executable(test_uuid
test_uuid.cpp)
target_link_libraries(test_uuid PUBLIC uuid) 