/**
 * @file epoll_server.cpp
 * @brief Implementation of high-performance Linux epoll-based TCP server
 *
 * This file contains the implementation of the epoll_server class, providing
 * a scalable, event-driven TCP server using Linux's epoll mechanism.
 *
 * Implementation Details:
 * - Edge-triggered epoll for maximum performance
 * - Non-blocking I/O throughout the implementation
 * - Efficient batch event processing
 * - Automatic connection state management
 * - Graceful error handling and recovery
 *
 * Performance Optimizations:
 * - accept4() with SOCK_NONBLOCK for efficient connection handling
 * - Event buffer auto-sizing to handle connection bursts
 * - Partial send/receive handling for large data transfers
 * - Minimal memory allocations in hot paths
 */

// check if we are on linux and platform that supports epoll

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#if defined(__linux__) || defined(__linux)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define EPOLLET 0
#define EPOLLEXCLUSIVE 0
#define EPOLLWAKEUP 0
#endif

#include <cstdlib>
#include <iostream>
#include <functional>
#include <thread>

#include "../includes/epoll_server.hpp"
#include "../includes/port.hpp"
#include "../includes/socket_address.hpp"
#include "../includes/utilities.hpp"
#include "../includes/file_descriptor.hpp"

namespace hh_socket
{
    void epoll_server::try_accept()
    {

        // Accept as many connections as possible (edge-triggered)
        while (true)
        {
            try
            {
                sockaddr_storage client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

#if defined(__linux__) || defined(__linux)
                // Use accept4 for efficiency (sets NONBLOCK + CLOEXEC atomically)
                auto cfd = ::accept4(listener_socket->get_fd(),
                                     reinterpret_cast<sockaddr *>(&client_addr),
                                     &client_addr_len,
                                     SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (cfd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break; // No more connections to accept
                    break;     // Max File Descriptors reached, no need to raise exception
                }
#else
                // Fallback windows implementation

                // Fallback Unix implementation
                int cfd = ::accept(listener_socket->get_fd(),
                                   reinterpret_cast<sockaddr *>(&client_addr),
                                   &client_addr_len);
                if (cfd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break; // No more connections to accept
                    break;     // Max File Descriptors reached, no need to raise exception
                }
                // Set non-blocking and close-on-exec flags
                u_long mode = 1;
                if (ioctlsocket(cfd, FIONBIO, &mode) != 0)
                {
                    throw socket_exception("Failed to set socket non-blocking mode: " + std::string(get_error_message()), "SocketOption", __func__);
                }
#endif

                // Optional: disable Nagle for latency-sensitive workloads.
                // int one = 1; setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

                // Add new connection to epoll monitoring
                if (add_epoll(cfd, EPOLLIN | EPOLLET) < 0)
                {
                    close_socket(cfd);
                    throw std::runtime_error("epoll_ctl ADD conn error: " + std::string(strerror(errno)));
                    continue;
                }

                // Create connection object and add to tracking
                auto connptr = std::make_shared<connection>(file_descriptor(cfd),
                                                            listener_socket->get_bound_address(),
                                                            socket_address(client_addr));
                current_open_connections++;
                conns.emplace(cfd, epoll_connection{connptr, {}, false});

                on_connection_opened(connptr);
            }
            catch (const std::exception &e)
            {
                on_exception_occurred(e);
                // Continue accepting other connections despite individual failures
            }
        }
    }

    void epoll_server::try_read(epoll_connection &c)
    {
        try
        {
            char buf[64 * 1024]; // 64KB buffer for high throughput
            std::size_t sz = 64 * 1024;
            int fd = c.conn->get_fd();
            // Read as much data as possible (edge-triggered)
            while (!c.want_close)
            {
                std::size_t m = ::recv(fd, buf, sz, 0);
                if (m > 0)
                {
                    on_message_received(c.conn, data_buffer(buf, m));
                }
                else if (m == 0)
                {
                    // Peer closed connection gracefully
                    close_conn(fd);
                    return;
                }
                else
                {
                    // Error or would block
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break; // No more data available
                    // Connection error, close it
                    close_conn(fd);
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            on_exception_occurred(e);
        }
    }
#if defined(__linux__) || defined(__linux)

    /**
     * Implementation Details:
     * - Uses setrlimit() system call with RLIMIT_NOFILE
     * - Sets both soft and hard limits to the same value
     * - Essential for high-concurrency servers
     */
    int epoll_server::set_rlimit_nofile(rlim_t soft, rlim_t hard)
    {
        struct rlimit rl{soft, hard};
        return setrlimit(RLIMIT_NOFILE, &rl);
    }
#endif
    /**
     * Implementation Notes:
     * - Uses EPOLL_CTL_ADD operation
     * - Stores file descriptor in event data for easy retrieval
     * - Edge-triggered mode requires careful handling of partial I/O
     */
    int epoll_server::add_epoll(int fd, uint32_t ev)
    {
        epoll_event e{};
        e.events = ev;
        e.data.fd = fd;

        return ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e);
    }

    /**
     * Usage Examples:
     * - Enable write monitoring: mod_epoll(fd, EPOLLIN | EPOLLOUT | EPOLLET)
     * - Disable write monitoring: mod_epoll(fd, EPOLLIN | EPOLLET)
     */
    int epoll_server::mod_epoll(int fd, uint32_t ev)
    {
        epoll_event e{};
        e.events = ev;
        e.data.fd = fd;
        // std::cout << fd << " " << ev << std::endl;
        return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &e);
    }

    /**
     * Implementation Notes:
     * - Uses EPOLL_CTL_DEL operation
     * - Third parameter can be NULL for delete operations
     * - Should be called before closing the file descriptor
     */
    int epoll_server::del_epoll(int fd)
    {
        return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    }

    /**
     * @param fd File descriptor of the connection to close
     *
     * Implementation Notes:
     * - Order of operations is important for proper cleanup
     * - Callbacks are called before resource deallocation
     * - Uses utility function close_socket() for cross-platform compatibility
     */
    void epoll_server::close_conn(int fd)
    {
        current_open_connections--;
        del_epoll(fd);
        on_connection_closed(conns[fd].conn);
        close_socket(fd);
        conns.erase(fd);
    }

    /**
     * Algorithm:
     * 1. Process each queued message in order
     * 2. Skip empty messages (cleanup)
     * 3. Attempt to send data using ::send()
     * 4. Handle partial sends by updating message content
     * 5. Stop on EAGAIN/EWOULDBLOCK (socket buffer full)
     * 6. Return false on any other error
     *
     * Edge Cases Handled:
     * - Empty messages in queue
     * - Partial sends (socket buffer full)
     * - Connection errors during send
     * - Exception safety with try-catch
     */
    bool epoll_server::flush_writes(epoll_connection &c)
    {
        try
        {
            while (!c.outq.empty())
            {
                std::string &front = c.outq.front();
                if (front.empty())
                {
                    c.outq.pop_front();
                    continue;
                }
                std::size_t n = ::send(c.conn->get_fd(), front.data(), front.size(), 0);
                if (n > 0)
                {
                    front.erase(0, (size_t)n);
                    continue;
                }
                if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    // Cannot write more now - socket buffer is full
                    return false;
                }
                // Error occurred during send operation
                return false;
            }
            return true;
        }
        catch (const std::exception &e)
        {
            on_exception_occurred(e);
            return false;
        }
    }

    /**
     * Event Loop Algorithm:
     * 1. Wait for events using epoll_wait()
     * 2. Handle timeout and error conditions
     * 3. Auto-resize event buffer if needed
     * 4. Process each ready event:
     *    - New connections (listener socket)
     *    - Incoming data (EPOLLIN)
     *    - Socket ready for writing (EPOLLOUT)
     *    - Connection errors/closures
     * 5. Repeat until stop signal
     *
     * Performance Features:
     * - Edge-triggered epoll for minimal syscalls
     * - Batch event processing
     * - Non-blocking accept loop for connection bursts
     * - Intelligent write flow control
     * - Dynamic event buffer sizing
     *
     * Error Handling:
     * - Graceful degradation on individual connection errors
     * - Exception isolation prevents server crashes
     * - Automatic cleanup of failed connections
     */
    void epoll_server::epoll_loop(int timeout)
    {
        on_listen_success();
        while (!g_stop)
            try
            {
                on_waiting_for_activity();
                // Wait for events with specified timeout
                int n = epoll_wait(epoll_fd, events.data(), (int)events.size(), timeout);
                if (n < 0)
                {
                    if (errno == EINTR)
                        continue; // Interrupted by signal, continue
                    // Fatal error in epoll_wait
                    on_exception_occurred(std::runtime_error("epoll_wait failed: " + std::string(strerror(errno))));
                    break;
                }

                // Auto-resize event buffer if saturated (indicates high load)
                if (n == (int)events.size())
                {
                    // grow event buffer if saturated
                    events.resize(events.size() * 2);
                }
                // Process each ready event
                for (int i = 0; i < n; ++i)
                {
                    uint32_t ev = events[i].events;
                    int fd = events[i].data.fd;

                    // Handle new connections on listener socket
                    if (listener_socket && fd == listener_socket->get_fd())
                    {
                        try_accept();
                        continue;
                    }

                    // Find connection state for this file descriptor
                    auto it = conns.find(fd);
                    if (it == conns.end())
                    {
                        continue; // Connection not found, skip
                    }
                    epoll_connection &c = it->second;

                    // Handle write flow control and output queue management
                    if (!c.outq.empty())
                    {
                        // Try to flush immediately to avoid extra EPOLLOUT wakeup
                        if (flush_writes(c))
                        {
                            // All data sent, disable write monitoring if enabled
                            if (c.want_write)
                            {
                                c.want_write = false;
                                mod_epoll(fd, EPOLLIN | EPOLLET);
                            }
                        }
                        else
                        {
                            // Data remains, ensure write monitoring is enabled
                            if (!c.want_write)
                            {
                                c.want_write = true;
                                mod_epoll(fd, EPOLLIN | EPOLLOUT | EPOLLET);
                            }
                        }
                    }

                    // Handle socket ready for writing (EPOLLOUT)
                    if (ev & EPOLLOUT)
                    {
                        if (flush_writes(c))
                        {
                            // All data sent, disable write monitoring
                            c.want_write = false;
                            mod_epoll(fd, EPOLLIN | EPOLLET);
                        }
                        // If flush_writes returns false, keep EPOLLOUT enabled
                    }

                    // Handle connection errors and closures
                    if (ev & (EPOLLERR | EPOLLHUP))
                    {
                        if (!c.want_write)
                            close_conn(fd);
                        continue;
                    }

                    // Handle custom close events (requested by application)
                    if (ev & HAMZA_CUSTOM_CLOSE_EVENT)
                    {
                        if (!c.want_write)
                            close_conn(fd);
                        continue;
                    }

                    // Handle incoming data (EPOLLIN)
                    if (ev & EPOLLIN)
                    {
                        try_read(c);
                    }

                next_event:
                    continue;
                }
                // After processing all events, you try to accept the connections that failed
                if (listener_socket)
                    try_accept();
            }
            catch (const std::exception &e)
            {
                std::cerr << "UNKNOWN ERROR CAUGHT BY EVENT LOOP: " << e.what() << std::endl;
                on_exception_occurred(e);
            }

        on_shutdown_success();
    }

    // ============================================================================
    // Protected Methods Implementation - TCP Server Interface
    // ============================================================================

    /**
     * Implementation Details:
     * - Uses custom epoll event (HAMZA_CUSTOM_CLOSE_EVENT) for signaling
     * - Defers actual closure to the main event loop
     * - Handles case where connection might already be closed
     * - Thread-safe approach via epoll signaling mechanism
     */
    void epoll_server::close_connection(std::shared_ptr<connection> conn)
    {
        auto c = conns.find(conn->get_fd());
        int fd = conn->get_fd();
        if (c == conns.end())
            return; // Connection already closed
        conns[fd].want_close = true;
        mod_epoll(fd, HAMZA_CUSTOM_CLOSE_EVENT);
    }

    void epoll_server::stop_reading_from_connection(std::shared_ptr<connection> conn)
    {
        auto c = conns.find(conn->get_fd());
        if (c != conns.end())
        {
            c->second.want_close = true;
        }
    }

    void epoll_server::close_connection(int fd)
    {
        auto c = conns.find(fd);
        if (c == conns.end())
            return; // Connection already closed
        conns[fd].want_close = true;
        mod_epoll(fd, HAMZA_CUSTOM_CLOSE_EVENT);
    }

    /**
     * @brief Queues a message for asynchronous sending
     *
     * Adds a message to the connection's output queue and ensures the connection
     * is monitored for write availability. Messages are sent asynchronously
     * when the socket becomes writable, providing efficient flow control.
     * Algorithm:
     * 1. Find connection in internal map
     * 2. Add message to connection's output queue
     * 3. Enable write monitoring via epoll
     * 4. Actual sending happens in main event loop
     *
     * Benefits:
     * - Non-blocking message queuing
     * - Automatic flow control via epoll
     * - Prevents blocking on full socket buffers
     * - Maintains message ordering per connection
     */
    void epoll_server::send_message(std::shared_ptr<connection> conn, const data_buffer &db)
    {
        int fd = conn->get_fd();
        auto it = conns.find(fd);
        if (it == conns.end())
        {
            return; // Connection not found
        }
        epoll_connection &c = it->second;
        c.outq.emplace_back(db.to_string());

        // Enable write monitoring to flush the queue
        mod_epoll(fd, EPOLLOUT);
    }

    // ============================================================================
    // Virtual Callback Methods - Override Points for Derived Classes
    // ============================================================================

    /**
     * Override Examples:
     * - Log to file or syslog
     * - Send error notifications
     * - Implement circuit breaker patterns
     * - Trigger failover mechanisms
     */
    void epoll_server::on_exception_occurred(const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    void epoll_server::on_connection_opened(std::shared_ptr<connection> conn)
    {
        std::cout << "Client Connected:\n";
        std::cout << "\t Client " << conn->get_fd() << " connected." << std::endl;
    }

    void epoll_server::on_connection_closed(std::shared_ptr<connection> conn)
    {
        std::cout << "Client Disconnected:\n";
        std::cout << "\t Client " << conn->get_fd() << " disconnected." << std::endl;
    }

    /**
     * Production Considerations:
     * - Default thread-per-message approach doesn't scale well
     * - Consider using thread pools for production servers
     * - Implement proper message parsing and validation
     * - Add rate limiting and abuse prevention
     *
     * Threading Notes:
     * - Uses lambda capture to safely pass shared_ptr
     * - Detached threads prevent blocking the main event loop
     * - Connection lifetime managed by shared_ptr reference counting
     */
    void epoll_server::on_message_received(std::shared_ptr<connection> conn, const data_buffer &db)
    {

        std::cout
            << "Message Received from " << conn->get_fd() << ": " << db.to_string() << std::endl;
        std::string message = "Echo " + db.to_string();

        if (db.to_string() == "close\n")
            close_connection(conn);
        else
            send_message(conn, data_buffer(message));
    }

    /**
     * Common Override Uses:
     * - Service discovery registration
     * - Startup notifications to monitoring systems
     * - Loading configuration or cached data
     * - Initializing background tasks
     */
    void epoll_server::on_listen_success()
    {
        std::cout << "Listening on " << listener_socket->get_fd() << std::endl;
    }

    /**
     * Common Override Uses:
     * - Final cleanup of application resources
     * - Shutdown notifications to monitoring systems
     * - Service discovery deregistration
     * - Persistent state saving
     */
    void epoll_server::on_shutdown_success()
    {
        std::cout << "Server Shutdown Successful" << std::endl;
    }

    // ============================================================================
    // Public Methods Implementation - Main Server Interface
    // ============================================================================

    /**
     * Initialization Steps:
     * 1. Configure process file descriptor limits
     * 2. Allocate initial event buffer (4096 events)
     * 3. Create epoll instance with EPOLL_CLOEXEC flag
     * 4. Validate epoll creation success
     */
    epoll_server::epoll_server(int max_fds)
    {
#if defined(__linux__) || defined(__linux)
        if (set_rlimit_nofile(max_fds, max_fds) != 0)
        {
            std::cerr << "Failed to set file descriptor limits: " << strerror(errno) << std::endl;
        }
        else

            this->max_fds = max_fds;
        events = std::vector<epoll_event>(4096);
        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd == -1)
        {
            std::cerr << "Failed to create epoll instance: " << strerror(errno) << std::endl;
            throw std::runtime_error("Failed to create epoll instance");
        }
#else
        events = std::vector<epoll_event>(4096);
        epoll_fd = epoll_create1(0);
        if (epoll_fd == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Failed to create epoll instance: " << strerror(errno) << std::endl;
            throw std::runtime_error("Failed to create epoll instance");
        }
#endif
    }

    void epoll_server::listen(int timeout)
    {
        epoll_loop(timeout);
    }

    /**
     * Prerequisites:
     * - Socket must be bound to an address
     * - Socket must be in listening state
     * - Socket should be configured with desired options
     *
     * Registration Process:
     * 1. Store socket reference internally
     * 2. Extract socket file descriptor
     * 3. Add socket to epoll monitoring with EPOLLIN | EPOLLET
     * 4. Return success/failure status
     *
     * @note Uses edge-triggered mode for maximum performance
     * @note Only one listening socket supported per server instance
     */
    bool epoll_server::register_listener_socket(std::shared_ptr<socket> sock_ptr)
    {
        listener_socket = sock_ptr;
        int lfd = sock_ptr->get_fd();
        if (add_epoll(lfd, EPOLLIN | EPOLLET) != 0)
        {
            return false;
        }
        return true;
    }

    /**

     * Sets the stop flag that will cause the main event loop to exit cleanly.
     * The server will finish processing current events before shutting down,
     * ensuring graceful closure of all connections.

     */
    void epoll_server::stop_server()
    {
        g_stop = 1;
    }

    /**

     * Cleanup Order:
     * 1. Close all active client connections
     * 2. Close listener socket if present
     * 3. Close epoll file descriptor
     */
    epoll_server::~epoll_server()
    {
        for (auto &[fd, _] : conns)
            close_socket(fd);
        if (listener_socket)
            close_socket(listener_socket->get_fd());
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        // hell nothing;
#else
        if (epoll_fd != -1)
            close_socket(epoll_fd);
#endif
    }
}

// #endif
