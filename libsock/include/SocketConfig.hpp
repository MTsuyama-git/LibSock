#pragma once
#include <sys/socket.h>

class SocketConfig {
private:
    SocketConfig(int protocolFamily, int connectionType, int ptorocol);
    int _protocolFamily;
    int _connectionType; // SOCK_STREAM or SOCK_DGRAM
    int _protocol; // IPPROTO_TCP or IPPROTO_UDP
public:
    static SocketConfig UdpConfig(int protocolFamily = PF_INET, int connectionType = SOCK_DGRAM);
    static SocketConfig TcpConfig(int protocolFamily = PF_INET, int connecitonType = SOCK_STREAM);
    int protocolFamily();
    int connectionType();
    int protocol();

};

