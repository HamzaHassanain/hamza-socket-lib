#pragma once

/**
 * @file tcp_server.hpp
 * @brief Abstract TCP server interface with comprehensive documentation.
 *
 * This header defines the abstract base class `tcp_server` which provides a
 * simple, documented interface that concrete server implementations must
 * implement. The class describes lifecycle hooks, I/O callbacks and control
 * methods used by implementations such as epoll-based or select-based servers.
 *
 * Design goals:
 * - Provide a minimal, clear interface for TCP server implementations.
 * - Separate transport-specific details (epoll, select, wepoll) from
 *   application-level callbacks (connections, messages, errors).
 * - Document expected behavior for each callback to make implementations
 *   and derived classes consistent and easy to use.
 *
 * @note This file only describes the interface. Concrete implementations
 *       must implement the pure virtual members defined below.
 *
 * @copyright Copyright (c) 2025
 */

#include "socket.hpp"
#include "connection.hpp"

namespace hh_socket
{

    /**
     * @brief Abstract interface for a TCP server implementation.
     *
     * The `tcp_server` class defines the callbacks and control operations a
     * concrete TCP server must provide. It is intentionally minimal so that
     * different I/O mechanisms (epoll, select, Windows IOCP, libuv, etc.) can
     * implement the interface while reusing common application logic.
     *
     * Responsibilities of a derived class:
     * - Manage the listening socket and accept incoming connections.
     * - Drive the I/O loop (platform-specific) and invoke the callbacks below
     *   when events occur.
     * - Ensure thread-safety where appropriate for the chosen design.
     *
     * Lifecycle and threading notes:
     * - listen() is expected to block and run the server loop until
     *   stop_server() is called or the server otherwise exits.
     * - Callbacks such as on_message_received may be called from the I/O
     *   thread; derived classes or users should document their threading
     *   guarantees if they change this behavior.
     */
    class tcp_server
    {

    protected:
        /**
         * @brief Request a connection to be closed.
         *
         * Implementations should provide a safe way to close user connections
         * triggered by the application. This method is intended to be called
         * by higher-level logic (for example, when the application wants to
         * terminate a client connection) and must ensure the closure happens in
         * a manner compatible with the underlying I/O loop (e.g. signalled to
         * the loop thread).
         *
         * @param conn Shared pointer to the connection to close.
         */
        virtual void close_connection(std::shared_ptr<connection> conn) = 0;

        /**
         * @brief Queue or send a message to a connection.
         *
         * Implementations must ensure that the provided buffer is sent to
         * the peer identified by `conn`. Sending may be asynchronous — the
         * implementation may choose to queue the data and write it when the
         * socket is writable. The method must not block indefinitely.
         *
         * @param conn Shared pointer to the target connection.
         * @param db  Data buffer containing the message to be delivered.
         */
        virtual void send_message(std::shared_ptr<connection> conn, const data_buffer &db) = 0;

        /**
         * @brief Notifies the server of an exception.
         *
         * Implementations should call this method to notify derived classes
         * about exceptions that occur in the I/O loop or during connection
         * handling. The default behavior (in concrete implementations) can
         * choose to log, cleanup, or propagate the error.
         *
         * @param e The exception object describing the error.
         */
        virtual void on_exception_occurred(const std::exception &e) = 0;

        /**
         * @brief Called when a new connection is opened.
         *
         * Invoked after a successful accept. Derived classes may use this
         * callback to initialize connection-specific state, perform
         * authentication, or register the connection with higher-level
         * application components.
         *
         * @param conn Shared pointer to the newly opened connection.
         */
        virtual void on_connection_opened(std::shared_ptr<connection> conn) = 0;

        /**
         * @brief Called when a connection has been closed.
         *
         * Invoked once a connection is fully closed and any associated
         * resources have been cleaned up by the server implementation. This
         * callback allows derived classes to release application-level
         * resources associated with the connection.
         *
         * @param conn Shared pointer to the closed connection.
         */
        virtual void on_connection_closed(std::shared_ptr<connection> conn) = 0;

        /**
         * @brief Called when a message is received from a connection.
         *
         * This callback receives the raw data as a `data_buffer`. The
         * derived class is responsible for parsing and acting upon the
         * contents. Implementations should document whether the buffer is
         * valid after the callback returns or must be copied for asynchronous
         * processing.
         *
         * @param conn Shared pointer to the connection that sent the data.
         * @param db   The received data buffer.
         */
        virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) = 0;

        /**
         * @brief Called when the server successfully begins listening.
         *
         * Invoked once the listening socket is ready and the server loop has
         * started. Derived classes may override to perform logging or
         * initialization that depends on the server being ready to accept
         * connections.
         */
        virtual void on_listen_success() = 0;

        /**
         * @brief Called after a successful shutdown of the server.
         *
         * Invoked when the server has completed graceful shutdown and all
         * resources have been released. Derived classes may override to
         * perform final cleanup or notify other components.
         */
        virtual void on_shutdown_success() = 0;

        /**
         * @brief Called when the server's I/O loop is waiting for activity.
         *
         * This hook is useful for instrumentation or low-traffic logging.
         * Derived classes may override to implement status reporting.
         */
        virtual void on_waiting_for_activity() = 0;

    public:
        /**
         * @brief Start the server event loop.
         *
         * This call typically blocks until the server is stopped. Implementors
         * should document whether listen() can be re-entered or if a new
         * server instance is required for subsequent runs.
         *
         * @param timeout Optional timeout value in milliseconds used by the
         *                underlying I/O wait (e.g. epoll_wait, select). A
         *                negative value may be used to indicate blocking
         *                behavior depending on the implementation.
         */
        virtual void listen(int timeout = 1000) = 0;

        /**
         * @brief Request the server to stop gracefully.
         *
         * The implementation should ensure the I/O loop exits cleanly and
         * that on_shutdown_success() is called once teardown is complete. The
         * method should be safe to call from other threads.
         */
        virtual void stop_server() = 0;
    };

} // namespace hh_socket
