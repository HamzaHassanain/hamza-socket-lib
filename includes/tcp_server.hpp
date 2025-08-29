#pragma once

/**
 * @file tcp_server.hpp
 * @brief The file for the abstract tcp_server class.

 *@note This class defines the interface for a TCP server.
 * @copyright Copyright (c) 2025
 *
 */

#include "socket.hpp"
#include "connection.hpp"

namespace hh_socket
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
        virtual void on_waiting_for_activity() = 0;

    public:
        virtual void listen(int timeout = 1000) = 0;

        virtual void stop_server() = 0;
    };

} // namespace
