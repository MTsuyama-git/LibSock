add_executable(test_tcp_server
    test_tcp_server.cpp)
target_link_libraries(test_tcp_server
    PUBLIC
    tcp
)

add_executable(websocket_sample
    websocket_sample.cpp)
target_link_libraries(websocket_sample
    PUBLIC
    tcp
    util
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