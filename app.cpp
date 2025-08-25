
#include "socket-lib.hpp"

#include <iostream>

class EchoServer : public hamza_socket::epoll_server
{
public:
    EchoServer() : hamza_socket::epoll_server(1000) {} // Max 1000 connections

protected:
    void on_connection_opened(std::shared_ptr<hamza_socket::connection> conn) override
    {
        std::cout << "Client connected from: " << conn->get_remote_address().to_string() << std::endl;
    }

    void on_message_received(std::shared_ptr<hamza_socket::connection> conn,
                             const hamza_socket::data_buffer &message) override
    {
        std::cout << "Received: " << message.to_string() << std::endl;
        // Echo the message back
        send_message(conn, message);
        close_connection(conn);
    }

    void on_connection_closed(std::shared_ptr<hamza_socket::connection> conn) override
    {
        std::cout << "Client disconnected: " << conn->get_remote_address().to_string() << std::endl;
    }

    void on_exception_occurred(const std::exception &e) override
    {
        std::cerr << "Server error: " << e.what() << std::endl;
    }

    void on_listen_success() override
    {
        std::cout << "Echo server started successfully!" << std::endl;
    }

    void on_shutdown_success() override
    {
        std::cout << "Server shutdown complete." << std::endl;
    }

    void on_waiting_for_activity() override
    {
        // Optional: periodic maintenance tasks
    }
};

int main()
{
    try
    {

        // Create TCP listening socket
        auto listener = hamza_socket::make_listener_socket(8080);

        // Create and run the echo server
        EchoServer server;
        if (server.register_listener_socket(listener))
        {
            server.listen(1000); // Start the server event loop
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}