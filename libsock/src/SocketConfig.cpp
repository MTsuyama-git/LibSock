#include <SocketConfig.hpp>
#include <arpa/inet.h>

SocketConfig::SocketConfig(int protocolFamily, int connecitonType, int protocol) : _protocolFamily(protocolFamily), _connectionType(connecitonType), _protocol(protocol) {}
SocketConfig SocketConfig::UdpConfig(int protoclFamily, int connectionType)
{
    return SocketConfig(protoclFamily, connectionType, IPPROTO_UDP);
}

SocketConfig SocketConfig::TcpConfig(int protoclFamily, int connectionType)
{
    return SocketConfig(protoclFamily, connectionType, IPPROTO_TCP);
}

int SocketConfig::protocolFamily()
{
    return _protocolFamily;
}

int SocketConfig::connectionType()
{
    return _connectionType;
}

int SocketConfig::protocol()
{
    return _protocol;
}