#include <TcpServer.hpp>
#include <BindConfig.hpp>

int main(int argc, char** argv)
{
    TcpServer server(BindConfig::BindConfig4Any(8080), SOCK_STREAM);
    return 0;
}