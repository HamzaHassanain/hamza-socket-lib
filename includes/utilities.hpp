#pragma once

#include <string>
#include <chrono>
#include <cstring> // For strerror
#include <memory>
// Platform detection and common socket types
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#ifndef SOCKET_PLATFORM_WINDOWS
#define SOCKET_PLATFORM_WINDOWS
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
using socklen_t = int;
const socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
const int SOCKET_ERROR_VALUE = SOCKET_ERROR;
#else
#ifndef SOCKET_PLATFORM_UNIX
#define SOCKET_PLATFORM_UNIX
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
using socket_t = int;
const socket_t INVALID_SOCKET_VALUE = -1;
const int SOCKET_ERROR_VALUE = -1;
#endif

namespace hh_socket
{
    // forward declarations
    class ip_address;
    class port;
    class family;
    class socket;
    class socket_address;
}

namespace hh_socket
{
    // Network address family constants
    const int IPV4 = AF_INET;  ///< IPv4 address family identifier
    const int IPV6 = AF_INET6; ///< IPv6 address family identifier

    // Port range constants
    const int MIN_PORT = 1024;  ///< Minimum valid port number (reserved)
    const int MAX_PORT = 65535; ///< Maximum valid port number

    // Buffer size constants for network operations
    const std::size_t DEFAULT_BUFFER_SIZE = 4 * 1024; ///< Default buffer size for socket I/O operations
    const std::size_t MAX_BUFFER_SIZE = 65536;        ///< Maximum buffer size for single network operations

    // Timeout constants (in milliseconds)
    const int DEFAULT_TIMEOUT = 5000;  ///< Default socket timeout for general operations
    const int CONNECT_TIMEOUT = 10000; ///< Connection establishment timeout
    const int RECV_TIMEOUT = 10000;    ///< Receive operation timeout

    // Socket queue constants
    const int DEFAULT_LISTEN_BACKLOG = SOMAXCONN; ///< Default listen queue size for servers

    /**
     * @brief Network protocol enumeration.
     *
     * Maps high-level protocol types to their corresponding socket types.
     * Used for socket creation and configuration.
     */
    enum class Protocol
    {
        TCP = SOCK_STREAM, ///< Transmission Control Protocol (reliable, connection-oriented)
        UDP = SOCK_DGRAM   ///< User Datagram Protocol (unreliable, connectionless)
    };

    /// Standard line terminator character for text protocols
    const char NEW_LINE = '\n';

    // Network address conversion utilities

    /**
     * @brief Convert IP address string to network byte order.
     * @param family_ip Address family (IPv4 or IPv6)
     * @param address IP address in string format (e.g., "192.168.1.1")
     * @param addr Output buffer to store network byte order address
     * @note Cross-platform implementation using inet_pton() or InetPtonA()
     * @note Output buffer must be large enough (4 bytes for IPv4, 16 bytes for IPv6)
     * @throws No exceptions - caller should validate return value
     */
    void convert_ip_address_to_network_order(const family &family_ip, const ip_address &address, void *addr);

    /**
     * @brief Extract IP address string from network address structure.
     * @param addr Network address structure (sockaddr_storage)
     * @return IP address in string format
     * @note Supports both IPv4 and IPv6 addresses
     * @note Uses inet_ntop() for cross-platform compatibility
     * @note Returns empty string on conversion failure
     */
    std::string get_ip_address_from_network_address(sockaddr_storage &addr);

    // Port management utilities

    /**
     * @brief Generate random available port number.
     * @return Random port in range 1024-65535 that is not currently in use
     * @note Thread-safe operation using internal mutex
     * @note Avoids well-known ports (0-1023) reserved for system services
     * @note May return same port if called repeatedly before binding
     */
    port get_random_free_port();

    /**
     * @brief Validate port number range.
     * @param p Port number to validate
     * @return true if port is in valid range (1-65535), false otherwise
     * @note Port 0 is considered invalid (though used for auto-assignment)
     * @note Does not check if port is actually available
     */
    bool is_valid_port(port p);

    /**
     * @brief Check if port is currently available for binding.
     * @param p Port number to check
     * @return true if port is free, false if in use
     * @note Implementation may vary by platform
     * @note Result may change between check and actual binding attempt
     */
    bool is_free_port(port p);

    // Byte order conversion utilities

    /**
     * @brief Convert port from host to network byte order.
     * @param port Port number in host byte order
     * @return Port number in network byte order (big-endian)
     * @note Uses htons() for cross-platform compatibility
     * @note Network protocols require big-endian byte order
     */
    int convert_host_to_network_order(int port);

    /**
     * @brief Convert port from network to host byte order.
     * @param port Port number in network byte order (big-endian)
     * @return Port number in host byte order
     * @note Uses ntohs() for cross-platform compatibility
     * @note Required when reading port numbers from network packets
     */
    int convert_network_order_to_host(int port);

    // Cross-platform socket management

    /**
     * @brief Initialize socket library (Windows-specific).
     * @return true if initialization successful, false otherwise
     * @note On Windows, initializes Winsock library (WSAStartup)
     * @note On Unix/Linux, always returns true (no initialization needed)
     * @note Must be called before any socket operations on Windows
     */
    bool initialize_socket_library();

    /**
     * @brief Cleanup socket library (Windows-specific).
     * @note On Windows, calls WSACleanup() to release Winsock resources
     * @note On Unix/Linux, does nothing
     * @note Should be called before program termination on Windows
     */
    void cleanup_socket_library();

    /**
     * @brief Close socket using platform-appropriate function.
     * @param socket Socket handle to close
     * @note On Windows, uses closesocket()
     * @note On Unix/Linux, uses close()
     * @note Does not validate socket handle before closing
     */
    void close_socket(socket_t socket);

    // Socket validation functions

    /**
     * @brief Check if socket handle is valid.
     * @param socket Socket handle to validate
     * @return true if socket handle is valid, false otherwise
     * @note On Windows, checks against INVALID_SOCKET
     * @note On Unix/Linux, checks if file descriptor >= 0
     * @note Does not verify if socket is actually open or connected
     */
    bool is_valid_socket(socket_t socket);

    /**
     * @brief Check if file descriptor represents an open socket.
     * @param fd File descriptor to check
     * @return true if fd is an open socket, false otherwise
     * @note Uses getsockopt() with SO_TYPE to verify socket type
     * @note Cross-platform implementation with appropriate casting
     * @note More reliable than simple range checking
     */
    bool is_socket_open(int fd);

    /**
     * @brief Check if socket is currently connected.
     * @param socket Socket handle to check
     * @return true if socket is connected to remote peer, false otherwise
     * @note Uses SO_ERROR to check for connection errors
     * @note Uses getpeername() to verify peer connection
     * @note Returns false for listening sockets or unconnected UDP sockets
     */
    bool is_socket_connected(socket_t socket);

    std::string get_error_message();

    /// @brief Convert string to uppercase.
    /// @param input String to convert
    /// @note does not modify the original string, returns a new uppercase string
    /// @return Uppercase version of input string
    std::string to_upper_case(const std::string &input);

    /**
     * @brief Create a listener socket.
     *
     * @param port Port number to listen on
     * @param ip IP address to bind to (default: "0.0.0.0")
     * @param backlog Maximum number of pending connections (default: SOMAXCONN)
     * @return std::shared_ptr<hh_socket::socket>
     */
    std::shared_ptr<hh_socket::socket> make_listener_socket(uint16_t port, const std::string &ip = "0.0.0.0", int backlog = SOMAXCONN);

}