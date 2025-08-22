#include <epoll_server.hpp>

int main()
{
    hamza_socket::epoll_server server(1024 * 1024); // Set max file descriptors to 1024
    auto listener = server.make_listener_socket(12345, "0.0.0.0", 1024 * 1024);
    if (!listener)
    {
        std::cerr << "Failed to create listener socket\n";
        return 1;
    }
    if (server.register_listener_socket(listener))
    {
        server.listen();
    }

    return 0;
}