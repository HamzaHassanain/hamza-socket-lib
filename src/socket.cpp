#include <iostream>
#include <stdexcept>
#include <cstring>
#include <thread>
// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <winsock2.h>
#include <ws2tcpip.h>
// #include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
// Windows doesn't have errno for socket operations, use WSAGetLastError()
#define socket_errno() WSAGetLastError()
#define SOCKET_WOULDBLOCK WSAEWOULDBLOCK
#define SOCKET_AGAIN WSAEWOULDBLOCK
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define socket_errno() errno
#define SOCKET_WOULDBLOCK EWOULDBLOCK
#define SOCKET_AGAIN EAGAIN
#endif

#include "../includes/socket.hpp"
#include "../includes/file_descriptor.hpp"
#include "../includes/utilities.hpp"
#include "../includes/exceptions.hpp"

namespace hh_socket
{

    socket::socket(const Protocol &protocol = Protocol::UDP)
        : protocol(protocol)
    {

        // Create socket: ::socket(domain, type, protocol)
        // domain: AF_INET (IPv4) or AF_INET6 (IPv6)
        // type: SOCK_STREAM (TCP) or SOCK_DGRAM (UDP)
        // protocol: Usually 0 for default protocol
        int socket_file_descriptor = ::socket(AF_INET, static_cast<int>(protocol), 0);

        // Check if socket creation succeeded (returns -1 on failure)
        if (!is_valid_socket(socket_file_descriptor))
        {
            throw socket_exception("Invalid File Descriptor", "SocketCreation", __func__);
        }

        fd = file_descriptor(socket_file_descriptor);
    }

    /**
     * Creates a new socket and binds it to the specified address.
     * Uses ::socket() system call to create socket with given family and protocol.
     * Validates socket creation and automatically binds to the provided address.
     */
    socket::socket(const socket_address &addr, const Protocol &protocol)
        : addr(addr)
    {

        // Create socket: ::socket(domain, type, protocol)
        // domain: AF_INET (IPv4) or AF_INET6 (IPv6)
        // type: SOCK_STREAM (TCP) or SOCK_DGRAM (UDP)
        // protocol: Usually 0 for default protocol
        int socket_file_descriptor = ::socket(addr.get_family().get(), static_cast<int>(protocol), 0);

        // Check if socket creation succeeded (returns -1 on failure)
        if (!is_valid_socket(socket_file_descriptor))
        {
            throw socket_exception("Invalid File Descriptor", "SocketCreation", __func__);
        }

        fd = file_descriptor(socket_file_descriptor);
        this->protocol = protocol;
        this->bind(addr); // Bind the socket to the address
    }

    /**
     * Establishes a connection to a remote server (TCP only).
     * Uses ::connect() system call to initiate connection to server_address.
     * For TCP: Creates a reliable, connection-oriented communication channel.
     * For UDP: This would set the default destination for send/receive operations.
     */
    void socket::connect(const socket_address &server_address)
    {
        // ::connect(sockfd, addr, addrlen) - connect socket to remote address
        // Returns 0 on success, -1 on error (sets errno)
        if (::connect(fd.get(), server_address.get_sock_addr(), server_address.get_sock_addr_len()) == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to connect to address: " + std::string(get_error_message()), "SocketConnection", __func__);
        }
    }

    /**
     * Binds the socket to a local address and port.
     * Uses ::bind() system call to associate socket with specific address.
     * Required for servers to listen on specific port, optional for clients.
     * For UDP servers: Necessary to receive packets on specific port.
     */
    void socket::bind(const socket_address &addr)
    {

        this->addr = addr;

        // ::bind(sockfd, addr, addrlen) - bind socket to local address
        // Returns 0 on success, -1 on error
        // Common errors: EADDRINUSE (port in use), EACCES (permission denied)
        if (::bind(fd.get(), this->addr.get_sock_addr(), this->addr.get_sock_addr_len()) == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to bind to address: " + std::string(get_error_message()), "SocketBinding", __func__);
        }
    }

    /**
     * Puts TCP socket into listening mode to accept incoming connections.
     * Uses ::listen() system call to mark socket as passive (server) socket.
     * backlog parameter specifies maximum number of pending connections in queue.
     * Only valid for TCP sockets (SOCK_STREAM).
     */
    void socket::listen(int backlog)
    {

        // Verify this is a TCP socket - UDP doesn't support listen/accept
        if (protocol != Protocol::TCP)
        {
            throw socket_exception("Listen is only supported for TCP sockets", "ProtocolMismatch", __func__);
        }

        // ::listen(sockfd, backlog) - listen for connections
        // backlog: max number of pending connections (typically SOMAXCONN)
        // Returns 0 on success, -1 on error
        if (::listen(fd.get(), backlog) == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to listen on socket: " + std::string(get_error_message()), "SocketListening", __func__);
        }
    }

    /**
     * Accepts an incoming TCP connection and creates a new socket for it.
     * Uses ::accept() system call to extract first pending connection.
     * Returns a new socket object representing the client connection.
     * Original socket remains in listening state for more connections.
     */
    std::shared_ptr<connection> socket::accept(bool NON_BLOCKING)
    {
        // Verify this is a TCP socket - UDP doesn't have connections to accept
        if (protocol != Protocol::TCP)
        {
            throw socket_exception("Accept is only supported for TCP sockets", "ProtocolMismatch", __func__);
        }
        if (fd.get() == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Socket is not open", "SocketAcceptance", __func__);
        }
        sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // ::accept(sockfd, addr, addrlen) - accept pending connection
        // Returns new socket descriptor for the connection, -1 on error
        // Fills client_addr with client's address information
        socket_t client_fd;
        if (!NON_BLOCKING)
        {

            client_fd = ::accept(fd.get(), reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
            if (client_fd == INVALID_SOCKET_VALUE)
            {
                throw socket_exception("Failed to accept connection: " + std::string(get_error_message()), "SocketAcceptance", __func__);
            }
        }
        else
        {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
            // use non-blocking accept on Windows
            client_fd = ::accept(fd.get(), reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
            if (client_fd != INVALID_SOCKET)
            {
                u_long mode = 1; // 1 to enable non-blocking socket
                if (ioctlsocket(client_fd, FIONBIO, &mode) != 0)
                {
                    closesocket(client_fd);
                    client_fd = INVALID_SOCKET;
                    throw socket_exception("Failed to set non-blocking mode on accepted socket: " + std::string(get_error_message()), "SocketOption", __func__);
                }
            }
#else
            // Use non-blocking accept on UNIX
            client_fd = ::accept4(fd.get(), reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
        }
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
        {
            // no connection to accept
            return std::shared_ptr<connection>(nullptr);
        }

        if (!is_valid_socket(client_fd))
        {
            throw socket_exception("Failed to accept connection: " + std::string(get_error_message()), "SocketAcceptance", __func__);
        }

        return std::make_shared<connection>(file_descriptor(client_fd), this->get_bound_address(), socket_address(client_addr));
    }

    /**
     * Receives data from any client via UDP socket.
     * Uses ::recvfrom() system call to receive datagram and sender information.
     * UDP is connectionless - can receive from any sender.
     * Buffer size is set to 64KB to handle maximum UDP payload size.
     */
    data_buffer socket::receive(socket_address &client_addr)
    {

        // Verify this is a UDP socket - TCP uses receive_on_connection()
        if (protocol != Protocol::UDP)
        {
            throw socket_exception("receive is only supported for UDP sockets", "ProtocolMismatch", __func__);
        }

        sockaddr_storage sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);

        // Use 64KB buffer for UDP - theoretical max UDP payload is 65507 bytes
        char buffer[MAX_BUFFER_SIZE];

        // ::recvfrom(sockfd, buf, len, flags, src_addr, addrlen) - receive datagram
        // Returns number of bytes received, -1 on error
        // Fills sender_addr with sender's address information
        std::size_t bytes_received = ::recvfrom(fd.get(), buffer, sizeof(buffer), 0,
                                                reinterpret_cast<sockaddr *>(&sender_addr), &sender_addr_len);

        if (bytes_received == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to receive data: " + std::string(get_error_message()), "SocketReceive", __func__);
        }

        // Extract sender's address and return received data
        client_addr = socket_address(sender_addr);
        return data_buffer(buffer, static_cast<std::size_t>(bytes_received));
    }

    /**
     * Sends data to specific address via UDP socket.
     * Uses ::sendto() system call to send datagram to specified destination.
     * UDP is connectionless - each send specifies destination address.
     * Verifies all data was sent in single operation.
     */
    void socket::send_to(const socket_address &addr, const data_buffer &data)
    {
        // Verify this is a UDP socket - TCP uses send_on_connection()
        if (protocol != Protocol::UDP)
        {
            throw socket_exception("send_to is only supported for UDP sockets", "ProtocolMismatch", __func__);
        }

        // ::sendto(sockfd, buf, len, flags, dest_addr, addrlen) - send datagram
        // Returns number of bytes sent, -1 on error
        std::size_t bytes_sent = ::sendto(fd.get(), data.data(), data.size(), 0,
                                          addr.get_sock_addr(), addr.get_sock_addr_len());

        if (bytes_sent == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to send data: " + std::string(get_error_message()), "SocketSend", __func__);
        }

        // UDP should send all data in one operation - partial sends indicate problems
        if (static_cast<std::size_t>(bytes_sent) != data.size())
        {
            throw socket_exception("Partial send: only " + std::to_string(bytes_sent) +
                                       " of " + std::to_string(data.size()) + " bytes sent",
                                   "PartialSend", __func__);
        }
    }

    /**
     * returns the bound local address.
     */
    socket_address socket::get_bound_address() const
    {
        return addr;
    }

    /**
     * Returns the raw file descriptor value for advanced operations.
     * Should be used carefully as it bypasses the wrapper's safety mechanisms.
     */
    int socket::get_fd() const
    {

        return fd.get();
    }

    /**
     * Sets the close-on-exec flag for the socket.
     * When enabled, the socket will be automatically closed
     * when the process executes a new program.
     */
    void socket::set_close_on_exec(bool enable)
    {
        try
        {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
            HANDLE handle = reinterpret_cast<HANDLE>(this->fd.get());
            DWORD flags = 0;

            if (GetHandleInformation(handle, &flags))
            {
                if (enable)
                    SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0); // Disable inheritance
                else
                    SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT); // Enable inheritance
            }
            else
            {
                throw std::runtime_error("Failed to get handle information");
            }
#else

            int flags = fcntl(this->fd.get(), F_GETFD);
            if (flags != -1)
            {
                if (enable)
                    fcntl(this->fd.get(), F_SETFD, flags | FD_CLOEXEC);
                else
                    fcntl(this->fd.get(), F_SETFD, flags & ~FD_CLOEXEC);
            }
#endif
        }
        catch (const std::exception &e)
        {
            throw socket_exception("Error setting close-on-exec flag: " + std::string(e.what()), "SocketSetCloseOnExec", __func__);
        }
    }

    /**
     * Sets SO_REUSEADDR socket option to allow address reuse.
     * Prevents "Address already in use" errors when restarting servers.
     * Uses setsockopt() system call to modify socket behavior.
     * Should be called before bind() to be effective.
     */
    void socket::set_reuse_address(bool reuse)
    {

        int optval = reuse ? 1 : 0;

        // setsockopt(sockfd, level, optname, optval, optlen) - set socket option
        // SOL_SOCKET: socket level options
        // SO_REUSEADDR: allow reuse of local addresses
        // Returns 0 on success, -1 on error

        const char *optval_ptr = reinterpret_cast<const char *>(&optval);
        if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, optval_ptr, sizeof(optval)) == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to set SO_REUSEADDR option: " + std::string(get_error_message()), "SocketOption", __func__);
        }
    }

    /**
     * Sets socket to non-blocking or blocking mode.
     * Non-blocking sockets return immediately from I/O operations even if no data
     * is available, preventing the calling thread from being blocked.
     * Uses fcntl() on UNIX/Linux or ioctlsocket() on Windows.
     * Essential for implementing asynchronous I/O and event-driven servers.
     */
    void socket::set_non_blocking(bool enable)
    {

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        // Windows implementation using ioctlsocket
        u_long mode = enable ? 1 : 0;
        if (ioctlsocket(fd.get(), FIONBIO, &mode) != 0)
        {
            throw socket_exception("Failed to set socket non-blocking mode: " + std::string(get_error_message()), "SocketOption", __func__);
        }
#else
        // UNIX/Linux implementation using fcntl
        // Get current file descriptor flags
        int flags = fcntl(fd.get(), F_GETFL, 0);
        if (flags == -1)
        {
            throw socket_exception("Failed to get socket flags: " + std::string(get_error_message()), "SocketOption", __func__);
        }

        // Modify O_NONBLOCK flag based on enable parameter
        if (enable)
        {
            flags |= O_NONBLOCK; // Set non-blocking
        }
        else
        {
            flags &= ~O_NONBLOCK; // Clear non-blocking (set blocking)
        }

        // Apply modified flags back to file descriptor
        if (fcntl(fd.get(), F_SETFL, flags) == -1)
        {
            throw socket_exception("Failed to set socket non-blocking mode: " + std::string(get_error_message()), "SocketOption", __func__);
        }
#endif
    }

    void socket::set_option(int level, int optname, int optval)
    {

        // setsockopt(sockfd, level, optname, optval, optlen) - set socket option
        // Returns 0 on success, -1 on error
        const char *optval_ptr = reinterpret_cast<const char *>(&optval);
        if (setsockopt(fd.get(), level, optname, optval_ptr, sizeof(optval)) == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to set socket option: " + std::string(get_error_message()), "SocketOption", __func__);
        }
    }

    bool socket::is_connected() const
    {
        return is_open;
    }

    void socket::disconnect()
    {
        if (is_open)
        {
            // Close the socket and mark it as closed
            close_socket(fd.get());
            fd.invalidate();
            is_open = false;
        }
    }

    /**
     * No explicit resource management needed here, we trust the socket user to close the socket, when done
     */
    socket::~socket()
    {
        disconnect();
    }
}