#include <string>
#include <memory>
#include <random>
#include <mutex>
#include <cstring>
// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // Link Windows socket library
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#endif

#include "../includes/utilities.hpp"
#include "../includes/ip_address.hpp"
#include "../includes/family.hpp"
#include "../includes/port.hpp"
#include "../includes/socket_address.hpp"
#include "../includes/socket.hpp"

// Global mutex for thread-safe random port generation
std::mutex get_random_free_port_mutex;

namespace hh_socket
{

    /**
     * Generate thread-safe random available port number.
     * Uses Mersenne Twister generator with steady clock seed for good randomness.
     * Avoids well-known ports (0-1023) and searches for available ports.
     * Returns first available port found in range 1024-65535.
     */
    port get_random_free_port()
    {
        // Static random number generator - initialized once per program run
        // Uses steady_clock for non-predictable seed based on system uptime
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

        // Uniform distribution for port range 1024-65535
        // Avoids privileged ports (0-1023) reserved for system services
        static std::uniform_int_distribution<int> dist(1024, 65535);

        // Thread-safe access to random generator
        // Prevents race conditions when multiple threads request ports
        std::lock_guard<std::mutex> lock(get_random_free_port_mutex);

        port p;
        do
        {
            // Generate random port in valid range
            p = port(dist(rng));

            // Continue searching until we find a valid, available port
            // continue if port is invalid OR port is not free
        } while (!is_valid_port(p) || !is_free_port(p));

        return p;
    }

    /**
     * Validate port number is within acceptable range.
     * Checks if port is between 1 and 65535 (excludes port 0).
     * Port 0 has special meaning (auto-assignment) in socket programming.
     */
    bool is_valid_port(port p)
    {
        // Valid port range: 1-65535
        // Port 0 is technically valid but has special meaning (auto-assign)
        // Ports 1-1023 are privileged/well-known ports requiring special permissions
        return p.get() > 0 && p.get() < 65536;
    }

    /**
     * Check if port is currently available for binding.
     * Creates a temporary socket and attempts to bind to the specified port.
     * Tests both TCP and UDP protocols to ensure port is completely free.
     * Properly closes sockets and handles all error conditions.
     */
    bool is_free_port(port p)
    {
        // First validate the port number is in acceptable range
        if (!is_valid_port(p))
        {
            return false;
        }

        // Test both TCP and UDP protocols since ports are protocol-specific
        // A port might be free for TCP but in use for UDP, or vice versa

        // Test TCP port availability
        socket_t tcp_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!is_valid_socket(tcp_socket))
        {
            return false; // If we can't create socket, assume port is not available
        }

        // Set SO_REUSEADDR to avoid "Address already in use" errors
        // This allows immediate reuse of the port after testing
        int reuse = 1;
#if defined(SOCKET_PLATFORM_WINDOWS)
        setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
        setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        // Prepare sockaddr_in structure for binding test
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;                     // Bind to any available interface
        addr.sin_port = htons(static_cast<uint16_t>(p.get())); // Convert port to network byte order

        // Attempt to bind to the port
        bool tcp_available = (bind(tcp_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);

        // Always close the TCP socket regardless of bind result
        close_socket(tcp_socket);

        // If TCP bind failed, port is definitely not available
        if (!tcp_available)
        {
            return false;
        }

        // Test UDP port availability
        socket_t udp_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!is_valid_socket(udp_socket))
        {
            return false; // If we can't create UDP socket, assume port is not available
        }

        // Set SO_REUSEADDR for UDP socket as well
#if defined(SOCKET_PLATFORM_WINDOWS)
        setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
        setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        // Attempt to bind UDP socket to the same port
        bool udp_available = (bind(udp_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);

        // Always close the UDP socket
        close_socket(udp_socket);

        // Port is free only if both TCP and UDP binding succeeded
        return tcp_available && udp_available;
    }

    /**
     * Convert IP address string to binary network format.
     * Handles both IPv4 and IPv6 addresses with platform-specific implementations.
     * Uses inet_pton() on Unix/Linux and InetPtonA() on Windows for compatibility.
     */
    void convert_ip_address_to_network_order(const family &family_ip, const ip_address &address, void *addr)
    {
#if defined(SOCKET_PLATFORM_WINDOWS)
        // Windows implementation using InetPtonA() function
        if (family_ip == family(IPV4))
        {
            // Convert IPv4 address string to binary format
            IN_ADDR in_addr;
            if (InetPtonA(AF_INET, address.get().c_str(), &in_addr) == 1)
            {
                // Copy converted address to output buffer
                *(reinterpret_cast<IN_ADDR *>(addr)) = in_addr;
            }
            // Note: No error handling for conversion failure
        }
        else if (family_ip == family(IPV6))
        {
            // Convert IPv6 address string to binary format
            IN6_ADDR in6_addr;
            if (InetPtonA(AF_INET6, address.get().c_str(), &in6_addr) == 1)
            {
                // Copy converted address to output buffer
                *(reinterpret_cast<IN6_ADDR *>(addr)) = in6_addr;
            }
            // Note: No error handling for conversion failure
        }
#else
        // Unix/Linux implementation using standard inet_pton()
        // Handles both IPv4 and IPv6 based on family parameter
        // Returns 1 on success, 0 for invalid format, -1 for unsupported family
        inet_pton(family_ip.get(), address.get().c_str(), addr);
        // Note: Return value not checked - caller should validate
#endif
    }

    /**
     * Extract human-readable IP address from network address structure.
     * Supports both IPv4 and IPv6 addresses using inet_ntop() for conversion.
     * Uses sockaddr_storage for generic address storage across address families.
     */
    std::string get_ip_address_from_network_address(struct sockaddr_storage &addr)
    {
        // Buffer large enough for both IPv4 and IPv6 string representations
        // INET6_ADDRSTRLEN = 46 bytes (maximum IPv6 string length)
        char ip_str[INET6_ADDRSTRLEN];

        if (addr.ss_family == AF_INET)
        {
            // Handle IPv4 address conversion
            // Cast generic storage to IPv4-specific structure
            sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(&addr);

            // Convert binary IPv4 address to string format
            inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
        }
        else if (addr.ss_family == AF_INET6)
        {
            // Handle IPv6 address conversion
            // Cast generic storage to IPv6-specific structure
            sockaddr_in6 *addr_in6 = reinterpret_cast<sockaddr_in6 *>(&addr);

            // Convert binary IPv6 address to string format
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str, sizeof(ip_str));
        }

        // Return string representation of IP address
        // Returns empty/garbage string if conversion fails or unsupported family
        return std::string(ip_str);
    }

    /**
     * Convert port number from host byte order to network byte order.
     * Network protocols require big-endian byte order regardless of host architecture.
     * Uses htons() (host-to-network-short) for cross-platform compatibility.
     */
    int convert_host_to_network_order(int port)
    {
        // htons() converts 16-bit value from host to network byte order
        // Available on both Windows (after Winsock) and Unix/Linux
        // Cast to uint16_t ensures proper 16-bit port handling
        return htons(static_cast<uint16_t>(port));
    }

    /**
     * Convert port number from network byte order to host byte order.
     * Required when reading port numbers from network packets or structures.
     * Uses ntohs() (network-to-host-short) for cross-platform compatibility.
     */
    int convert_network_order_to_host(int port)
    {
        // ntohs() converts 16-bit value from network to host byte order
        // Available on both Windows (after Winsock) and Unix/Linux
        // Cast to uint16_t ensures proper 16-bit port handling
        return ntohs(static_cast<uint16_t>(port));
    }

    /**
     * Initialize socket library for cross-platform compatibility.
     * Windows requires Winsock initialization before any socket operations.
     * Unix/Linux systems have sockets built into the kernel - no initialization needed.
     */
    bool initialize_socket_library()
    {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        // Windows Winsock initialization
        WSADATA wsaData;

        // Request Winsock version 2.2 (MAKEWORD(2, 2))
        // wsaData structure receives implementation details
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

        // WSAStartup returns 0 on success, error code on failure
        return result == 0;
#else
        // Unix/Linux doesn't require socket library initialization
        // Sockets are part of the kernel and always available
        return true;
#endif
    }

    /**
     * Cleanup socket library resources.
     * Windows requires explicit Winsock cleanup to release resources.
     * Unix/Linux systems handle socket cleanup automatically.
     */
    void cleanup_socket_library()
    {
#if defined(SOCKET_PLATFORM_WINDOWS)
        // Release Winsock resources and terminate socket usage
        // Should be called before program termination
        WSACleanup();
#else
        // Unix/Linux doesn't require explicit socket library cleanup
        // Kernel handles resource cleanup automatically at process termination
#endif
    }

    /**
     * Close socket using platform-appropriate function.
     * Windows and Unix/Linux use different function names for closing sockets.
     * Abstracts the platform differences for portable code.
     */
    void close_socket(socket_t socket)
    {
#if defined(SOCKET_PLATFORM_WINDOWS)
        // Windows uses closesocket() for socket handles
        // Different from CloseHandle() used for other Windows objects
        closesocket(socket);
#else
        // Unix/Linux treats sockets as file descriptors
        // Uses standard close() system call
        close(socket);
        // makesure the fd is closed and reusable

#endif
    }

    /**
     * Validate socket handle using platform-specific criteria.
     * Windows and Unix/Linux use different invalid socket values.
     * Does not verify if socket is actually open or usable.
     */
    bool is_valid_socket(socket_t socket)
    {
#if defined(SOCKET_PLATFORM_WINDOWS)
        // Windows invalid socket constant is INVALID_SOCKET (~0)
        // Socket handles are unsigned integers on Windows
        return socket != INVALID_SOCKET;
#else
        // Unix/Linux invalid socket value is -1
        // Socket descriptors are signed integers (file descriptors)
        return socket >= 0;
#endif
    }

    /**
     * Verify file descriptor represents an open socket.
     * Uses getsockopt() with SO_TYPE to determine if fd is a socket.
     * More reliable than simple range checking for socket validation.
     */
    bool is_socket_open(int fd)
    {
#if defined(SOCKET_PLATFORM_WINDOWS)
        // Windows implementation with proper type casting
        int type;
        int type_len = sizeof(type);

        // getsockopt() returns 0 on success for valid socket
        // SO_TYPE option retrieves socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
        // Windows requires char* cast for getsockopt buffer parameter
        return getsockopt(fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char *>(&type), &type_len) == 0;
#else
        // Unix/Linux implementation with POSIX-compliant types
        int type;
        socklen_t type_len = sizeof(type);

        // getsockopt() returns 0 on success for valid socket
        // Uses socklen_t type as required by POSIX specification
        return getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_len) == 0;
#endif
    }

    /**
     * Determine if socket is connected to a remote peer.
     * Performs comprehensive connection state checking using multiple methods.
     * Validates socket errors and attempts to retrieve peer address.
     */
    bool is_socket_connected(socket_t socket)
    {
        // First validate that socket handle is valid
        if (!is_valid_socket(socket))
        {
            return false;
        }

#if defined(SOCKET_PLATFORM_WINDOWS)
        // Windows connection state checking
        int error = 0;
        int error_len = sizeof(error);

        // Check for pending socket errors using SO_ERROR option
        // SO_ERROR returns and clears the last error that occurred on socket
        int result = getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &error_len);

        // If getsockopt failed or there's a pending error, socket is not properly connected
        if (result != 0 || error != 0)
        {
            return false;
        }

        // Try to get peer address as final verification
        // getpeername() succeeds only if socket is connected to remote peer
        sockaddr_storage addr;
        int addr_len = sizeof(addr);
        return getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addr_len) == 0;
#else
        // Unix/Linux connection state checking
        int error = 0;
        socklen_t error_len = sizeof(error);

        // Check for pending socket errors using SO_ERROR option
        // Similar to Windows but uses POSIX-compliant socklen_t type
        int result = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &error_len);

        // If getsockopt failed or there's a pending error, socket is not properly connected
        if (result != 0 || error != 0)
        {
            return false;
        }

        // Try to get peer address as final verification
        // getpeername() succeeds only if socket is connected to remote peer
        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        return getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addr_len) == 0;
#endif
    }

    std::string get_error_message()
    {
        {
#if defined(SOCKET_PLATFORM_WINDOWS)
            return std::to_string(WSAGetLastError());
#else
            return std::string(strerror(errno));
#endif
        }
    }

    std::string to_upper_case(const std::string &input)
    {
        std::string upper_case_str = input;
        std::transform(upper_case_str.begin(), upper_case_str.end(), upper_case_str.begin(),
                       [](unsigned char c)
                       { return std::toupper(c); });
        return upper_case_str;
    }

    std::shared_ptr<hh_socket::socket> make_listener_socket(uint16_t port, const std::string &ip, int backlog)
    {
        try
        {
            auto sock_ptr = std::make_shared<hh_socket::socket>(hh_socket::Protocol::TCP);

            sock_ptr->set_reuse_address(true);
            sock_ptr->set_non_blocking(true);
            sock_ptr->set_close_on_exec(true);
            sock_ptr->bind(hh_socket::socket_address(hh_socket::port(port), hh_socket::ip_address(ip)));
            sock_ptr->listen(backlog);

            // Optional: reduce wakeups by notifying only when data arrives (Linux-specific).
            // int defer_secs = 1;
            // sock_ptr->set_option(IPPROTO_TCP, TCP_DEFER_ACCEPT, defer_secs);

            return sock_ptr;
        }
        catch (socket_exception &e)
        {
            throw std::runtime_error("Failed to create listener socket: " + e.what());
        }
    }
}