add_executable(test_tcp_server
    test_tcp_server.cpp)
target_link_libraries(test_tcp_server
    PUBLIC
    tcp
)

add_executable(test_tcp_client
    test_tcp_client.cpp)
target_link_libraries(test_tcp_client
    PUBLIC
    tcp
)