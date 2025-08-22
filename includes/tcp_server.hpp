#pragma once

#include <socket.hpp>
#include <connection.hpp>

namespace hamza_socket
{

    class tcp_server
    {

    protected:
        virtual void close_connection(std::shared_ptr<connection> conn) = 0;
        virtual void send_message(std::shared_ptr<connection> conn, const data_buffer &db) = 0;
        virtual void on_exception_occurred(const std::exception &e) = 0;
        virtual void on_connection_opened(std::shared_ptr<connection> conn) = 0;
        virtual void on_connection_closed(std::shared_ptr<connection> conn) = 0;
        virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) = 0;
        virtual void on_listen_success() = 0;
        virtual void on_shutdown_success() = 0;

    public:
        virtual void listen(int timeout = 1000) = 0;

        virtual void stop_server() = 0;
    };

} // namespace
