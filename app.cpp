#include <epoll_server.hpp>
#include <iostream>

int main()
{
    hamza_socket::epoll_server server(100);
    server.register_listener_socket(hamza_socket::make_listener_socket(8080));
    server.listen();
    return 0;
}