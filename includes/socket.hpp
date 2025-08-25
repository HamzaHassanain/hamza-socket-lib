#pragma once

#include <string>

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif
#include <atomic>

#include "socket_address.hpp"
#include "file_descriptor.hpp"
#include "data_buffer.hpp"
#include "utilities.hpp"
#include "exceptions.hpp"
#include "connection.hpp"

namespace hh_socket
{
    /**
     * @brief Cross-platform socket wrapper for TCP and UDP network operations.
     *
     * This class provides a high-level, interface for socket programming.
     * It abstracts system-level socket operations and handles resource management
     * automatically. Supports both TCP and UDP protocols with separate method sets
     * for connection-oriented and connectionless operations.
     *
     * Platform Support:
     * - Linux/Unix: Full support for all socket options
     * - Windows: Full support except TCP_QUICKACK (Windows-specific alternatives may vary)
     *
     * The class implements move-only semantics to ensure unique ownership of
     * socket resources and prevent accidental duplication or resource leaks.
     *
     * @note Uses explicit constructors to prevent implicit conversions
     * @note Move-only design prevents copying socket resources
     * @note Cross-platform compatibility with conditional compilation
     * @note Automatically handles socket cleanup in destructor
     */
    class socket
    {
    private:
        /// Socket address (IP, port, family)
        socket_address addr;

        /// Platform-specific file descriptor wrapper
        file_descriptor fd;

        /// Protocol type (TCP or UDP)
        Protocol protocol;

        /// flag to indicate if the socket is open
        bool is_open{true};

    public:
        /// Default constructor deleted - sockets must be explicitly configured
        socket() = delete;

        /**
         * @brief Create and bind socket to address.
         * @param protocol Network protocol (TCP/UDP)
         * @throws socket_exception with type "SocketCreation" if socket creation fails
         */
        explicit socket(const Protocol &protocol);

        /**
         * @brief Create and bind socket to address.
         * @param addr Socket address to bind to
         * @param protocol Network protocol (TCP/UDP)
         * @note Automatically binds socket to the specified address.
         * @throws socket_exception with type "SocketCreation" if socket creation fails
         * @throws socket_exception with type "SocketBinding" if binding fails
         */
        explicit socket(const socket_address &addr, const Protocol &protocol);

        // Copy operations - DELETED for resource safety
        socket(const socket &) = delete;
        socket &operator=(const socket &) = delete;

        /**
         * @brief Move constructor.
         * @param other Socket to move from
         *
         * Transfers ownership of socket resources. Source socket becomes invalid.
         */
        socket(socket &&other)
            : addr(std::move(other.addr)), fd(std::move(other.fd)), protocol(other.protocol) {}

        /**
         * @brief Move assignment operator.
         * @param other Socket to move from
         * @return Reference to this socket
         *
         * Transfers ownership of socket resources from another socket.
         */
        socket &operator=(socket &&other)
        {
            if (this != &other)
            {
                addr = std::move(other.addr);
                fd = std::move(other.fd);
                protocol = other.protocol;
            }
            return *this;
        }

        /**
         * @brief Binds socket to the specified address.
         * @param addr Address to bind to
         * @throws socket_exception with type "SocketBinding" if bind operation fails
         */
        void bind(const socket_address &addr);

        /**
         * @brief Sets SO_REUSEADDR socket option.
         * @param reuse Whether to enable address reuse
         * @throws socket_exception with type "SocketOption" if setsockopt fails
         */
        void set_reuse_address(bool reuse);

        /**
         * @brief Sets socket to non-blocking or blocking mode.
         * @param enable Whether to enable non-blocking mode
         * @throws socket_exception with type "SocketOption" if operation fails
         *
         * Cross-platform implementation:
         * - Unix/Linux: Uses fcntl() with O_NONBLOCK flag
         * - Windows: Uses ioctlsocket() with FIONBIO
         *
         * Non-blocking sockets return immediately from I/O operations even if no data
         * is available, preventing the calling thread from being blocked.
         * Essential for implementing asynchronous I/O and event-driven servers.
         */
        void set_non_blocking(bool enable);

        /**
         * @brief Sets SO_KEEPALIVE socket option to enable TCP keep-alive probes.
         * @param enable Whether to enable keep-alive
         * @throws socket_exception with type "ProtocolMismatch" if called on non-TCP socket
         * @throws socket_exception with type "SocketOption" if setsockopt fails
         *
         * When enabled, TCP automatically sends keep-alive packets to detect
         * dead connections and clean up resources for broken connections.
         * Only applicable to TCP sockets.
         */

        /**
         * @brief Set the close on exec object
         *  that prevents the file descriptor from being inherited by child processes.
         * @param enable Whether to enable close-on-exec
         */

        void set_close_on_exec(bool enable);

        void connect(const socket_address &addr);

        /**
         * @brief Start listening for connections (TCP only).
         * @param backlog Maximum number of pending connections
         * @throws socket_exception with type "ProtocolMismatch" if called on non-TCP socket
         * @throws socket_exception with type "SocketListening" if listen operation fails
         */
        void listen(int backlog = SOMAXCONN);

        /**
         * @brief Accept incoming connection (TCP only).
         * @param NON_BLOCKING Whether to use non-blocking accept for clients (default: false)
         * @return Shared pointer to new socket for the accepted connection
         * @throws socket_exception with type "ProtocolMismatch" if called on non-TCP socket
         * @throws socket_exception with type "SocketAcceptance" if accept operation fails
         */
        std::shared_ptr<connection> accept(bool NON_BLOCKING = false);

        /**
         * @brief Receive data from any client (UDP only).
         * @param client_addr Will be filled with sender's address
         * @return Buffer containing received data
         * @throws socket_exception with type "ProtocolMismatch" if called on non-UDP socket
         * @throws socket_exception with type "SocketReceive" if receive operation fails
         */
        data_buffer receive(socket_address &client_addr);

        /**
         * @brief Send data to specific address (UDP only).
         * @param addr Destination address
         * @param data Data to send
         * @throws socket_exception with type "ProtocolMismatch" if called on non-UDP socket
         * @throws socket_exception with type "SocketSend" if send operation fails
         * @throws socket_exception with type "PartialSend" if not all data was sent
         */
        void send_to(const socket_address &addr, const data_buffer &data);

        /**
         * @brief Get remote endpoint address.
         * @return Socket address of remote endpoint
         */
        socket_address get_bound_address() const;

        /**
         * @brief Get raw file descriptor value.
         * @return Integer file descriptor value
         */
        int get_fd() const;

        /**
         * @brief Close the socket connection.
         *
         * Safely closes the socket and releases system resources.
         * Socket becomes unusable after this call.
         * @note This method does not throw exceptions - errors are logged to stderr
         */
        void disconnect();

        /**
         * @brief Check if socket is connected.
         * @return true if socket has valid connection, false otherwise
         */
        bool is_connected() const;

        /**
         * @brief Less-than operator for container ordering.
         * @param other Socket to compare with
         * @return true if this socket's file descriptor is less than other's
         */

        /**
         * @brief Set custom options
         * @param level The level at which the option is defined (e.g., SOL_SOCKET)
         * @param optname The name of the option to set
         * @param optval The value to set the option to
         */
        void set_option(int level, int optname, int optval);

        bool operator<(const socket &other) const
        {
            return fd < other.fd;
        }

        /// Destructor - automatically handles resource cleanup
        ~socket();
    };
}