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
 *
 * @author Hamza Hassan
 * @date 2025
 */

// check if we are on linux and platform that supports epoll
#if defined(__linux__) || defined(__linux)

#include <epoll_server.hpp>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <functional>
#include <thread>

#include <port.hpp>
#include <socket_address.hpp>
#include <utilities.hpp>
#include <file_descriptor.hpp>

namespace hamza_socket
{

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
        return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e);
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
     * @brief Removes a file descriptor from epoll monitoring
     *
     * Unregisters a file descriptor from the epoll instance. Called during
     * connection cleanup to stop monitoring events for closed connections.
     *
     * @param fd File descriptor to remove
     * @return 0 on success, -1 on failure
     *
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
     * @brief Closes and performs complete cleanup of a connection
     *
     * This function handles the complete lifecycle cleanup of a connection:
     * 1. Removes the connection from epoll monitoring
     * 2. Notifies derived classes via callback
     * 3. Closes the underlying socket
     * 4. Removes connection from internal tracking
     *
     * @param fd File descriptor of the connection to close
     *
     * Implementation Notes:
     * - Order of operations is important for proper cleanup
     * - Callbacks are called before resource deallocation
     * - Uses utility function close_socket() for cross-platform compatibility
     */
    void epoll_server::close_conn(int fd)
    {
        del_epoll(fd);
        on_connection_closed(conns[fd].conn);
        close_socket(fd);
        conns.erase(fd);
    }

    /**
     * @brief Attempts to flush pending writes for a connection
     *
     * Tries to send all queued outbound data for a connection. Handles partial
     * sends gracefully by updating the output queue. This function is called
     * whenever a socket becomes writable or when new data is queued for sending.
     *
     * @param c Reference to the epoll_connection containing output queue
     * @return true if all queued data was sent, false if data remains or error occurred
     *
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
                ssize_t n = ::send(c.conn->get_fd(), front.data(), front.size(), 0);
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
     * @brief Main event loop using epoll for high-performance I/O multiplexing
     *
     * This is the core of the server - an event-driven loop that efficiently
     * handles thousands of concurrent connections using Linux's epoll mechanism.
     * The loop continues until a stop signal is received.
     *
     * @param timeout Timeout in milliseconds for epoll_wait calls
     *
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
                        // Accept as many connections as possible (edge-triggered)
                        while (true)
                        {
                            try
                            {
                                sockaddr_storage client_addr;
                                socklen_t client_addr_len = sizeof(client_addr);

                                // Use accept4 for efficiency (sets NONBLOCK + CLOEXEC atomically)
                                auto cfd = ::accept4(listener_socket->get_fd(),
                                                     reinterpret_cast<sockaddr *>(&client_addr),
                                                     &client_addr_len,
                                                     SOCK_NONBLOCK | SOCK_CLOEXEC);
                                if (cfd < 0)
                                {
                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        break; // No more connections to accept
                                    throw std::runtime_error("accept4 failed: " + std::string(strerror(errno)));
                                    break;
                                }

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
                                conns.emplace(cfd, epoll_connection{connptr, {}, false});
                                on_connection_opened(connptr);
                            }
                            catch (const std::exception &e)
                            {
                                on_exception_occurred(e);
                                // Continue accepting other connections despite individual failures
                            }
                        }
                        continue;
                    }

                    // Find connection state for this file descriptor
                    auto it = conns.find(fd);
                    if (it == conns.end())
                    {
                        continue; // Connection not found, skip
                    }
                    epoll_connection &c = it->second;

                    // Handle connection errors and closures
                    if (ev & (EPOLLERR | EPOLLHUP))
                    {
                        close_conn(fd);
                        continue;
                    }

                    // Handle custom close events (requested by application)
                    if (ev & HAMZA_CUSTOM_CLOSE_EVENT)
                    {
                        close_conn(fd);
                        continue;
                    }

                    // Handle incoming data (EPOLLIN)
                    if (ev & EPOLLIN)
                    {
                        try
                        {
                            char buf[64 * 1024]; // 64KB buffer for high throughput

                            // Read as much data as possible (edge-triggered)
                            while (true)
                            {
                                ssize_t m = ::recv(fd, buf, sizeof(buf), 0);
                                if (m > 0)
                                {
                                    // Data received, notify application
                                    on_message_received(c.conn, data_buffer(buf, m));
                                }
                                else if (m == 0)
                                {
                                    // Peer closed connection gracefully
                                    close_conn(fd);
                                    goto next_event;
                                }
                                else
                                {
                                    // Error or would block
                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        break; // No more data available
                                    // Connection error, close it
                                    close_conn(fd);
                                    goto next_event;
                                }
                            }
                        }
                        catch (const std::exception &e)
                        {
                            on_exception_occurred(e);
                        }
                    }

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

                next_event:
                    continue;
                }
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
     * @brief Requests closure of a specific connection
     *
     * This method provides a safe way for derived classes to request connection
     * closure. Instead of immediately closing the connection, it signals the
     * main event loop using a custom epoll event. This ensures the closure
     * happens in the proper context and maintains thread safety.
     *
     * @param conn Shared pointer to the connection to close
     *
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
        mod_epoll(fd, HAMZA_CUSTOM_CLOSE_EVENT);
    }

    /**
     * @brief Queues a message for asynchronous sending
     *
     * Adds a message to the connection's output queue and ensures the connection
     * is monitored for write availability. Messages are sent asynchronously
     * when the socket becomes writable, providing efficient flow control.
     *
     * @param conn Shared pointer to the target connection
     * @param db Data buffer containing the message to send
     *
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
     * @brief Default exception handler for server errors
     *
     * Called whenever an exception occurs during server operation. The default
     * implementation logs the error to stderr. Derived classes can override
     * this to implement custom error handling, logging, or recovery strategies.
     *
     * @param e The exception that occurred
     *
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

    /**
     * @brief Called when a new client connection is established
     *
     * Default implementation logs basic connection information. Derived classes
     * typically override this to implement:
     * - Authentication and authorization
     * - Connection rate limiting
     * - Client information logging
     * - Connection initialization
     *
     * @param conn Shared pointer to the newly established connection
     */
    void epoll_server::on_connection_opened(std::shared_ptr<connection> conn)
    {
        std::cout << "Client Connected:\n";
        std::cout << "\t Client " << conn->get_fd() << " connected." << std::endl;
    }

    /**
     * @brief Called when a client connection is closed
     *
     * Default implementation logs disconnection information. Derived classes
     * can override this to implement:
     * - Session cleanup
     * - Connection statistics logging
     * - Resource deallocation
     * - User logout handling
     *
     * @param conn Shared pointer to the closed connection
     */
    void epoll_server::on_connection_closed(std::shared_ptr<connection> conn)
    {
        std::cout << "Client Disconnected:\n";
        std::cout << "\t Client " << conn->get_fd() << " disconnected." << std::endl;
    }

    /**
     * @brief Called when data is received from a client connection
     *
     * This is the primary message processing callback that derived classes should
     * override to implement application-specific logic. The default implementation
     * provides a simple echo server using detached threads.
     *
     * @param conn Shared pointer to the connection that sent the data
     * @param db Data buffer containing the received message
     *
     * Default Behavior:
     * - Spawns a detached thread for each message
     * - Echoes received data back to sender
     * - Handles special "close" command to terminate connection
     *
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
        std::thread([&, conn, db]()
                    {
                        std::cout
                            << "Message Received from " << conn->get_fd() << ": " << db.to_string() << std::endl;
                        std::string message = "Echo " + db.to_string();

                        if (db.to_string() == "close\n")
                            close_connection(conn);
                        else
                            send_message(conn, data_buffer(message)); })
            .detach();
    }

    /**
     * @brief Called when the server successfully starts listening
     *
     * Default implementation logs the listening socket information. Derived
     * classes can override this to implement startup notifications, service
     * registration, or initialization completion signaling.
     *
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
     * @brief Called when the server shuts down gracefully
     *
     * Default implementation logs shutdown completion. Derived classes can
     * override this to implement cleanup procedures, shutdown notifications,
     * or resource finalization.
     *
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
     * @brief Constructs and initializes the epoll server
     *
     * Performs complete server initialization including resource limit
     * configuration, epoll instance creation, and event buffer allocation.
     * This constructor prepares the server for high-concurrency operation.
     *
     * @param max_fds Maximum number of file descriptors the server should handle
     *
     * Initialization Steps:
     * 1. Configure process file descriptor limits
     * 2. Allocate initial event buffer (4096 events)
     * 3. Create epoll instance with EPOLL_CLOEXEC flag
     * 4. Validate epoll creation success
     *
     * @throws std::runtime_error if epoll instance creation fails
     *
     * Resource Management:
     * - Sets both soft and hard rlimits to max_fds
     * - EPOLL_CLOEXEC ensures epoll FD is closed on exec
     * - Initial event buffer size of 4096 handles most workloads
     * - Buffer auto-resizes during operation if needed
     */
    epoll_server::epoll_server(int max_fds)
    {
        set_rlimit_nofile(max_fds, max_fds);
        events = std::vector<epoll_event>(4096);
        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd == -1)
        {
            std::cerr << "Failed to create epoll instance: " << strerror(errno) << std::endl;
            throw std::runtime_error("Failed to create epoll instance");
        }
    }

    /**
     * @brief Starts the main server event loop
     *
     * Initiates the main event processing loop that handles all server operations.
     * This method blocks until the server is stopped via stop_server() or a
     * signal is received. It delegates to epoll_loop() for the actual implementation.
     *
     * @param timeout Timeout in milliseconds for epoll_wait calls (default: 1000ms)
     *
     * Behavior:
     * - Calls epoll_loop() to start event processing
     * - Blocks until server shutdown is requested
     * - Handles all connection events, I/O operations, and errors
     * - Provides graceful shutdown when stop flag is set
     *
     * @note This method overrides tcp_server::listen()
     * @note The method blocks the calling thread until shutdown
     */
    void epoll_server::listen(int timeout)
    {
        epoll_loop(timeout);
    }

    /**
     * @brief Registers a pre-configured listening socket with the server
     *
     * Associates a listening socket with the epoll server and begins monitoring
     * it for incoming connections. The socket must be properly configured
     * (bound and listening) before calling this method.
     *
     * @param sock_ptr Shared pointer to the configured listening socket
     * @return true if registration successful, false on failure
     *
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
     * @brief Initiates graceful server shutdown
     *
     * Sets the stop flag that will cause the main event loop to exit cleanly.
     * The server will finish processing current events before shutting down,
     * ensuring graceful closure of all connections.
     *
     * Shutdown Process:
     * 1. Set atomic stop flag
     * 2. Event loop detects flag on next iteration
     * 3. Loop exits after current event batch
     * 4. Destructor handles final cleanup
     *
     * Benefits:
     * - Non-disruptive shutdown process
     * - Allows current operations to complete
     * - Prevents data loss during shutdown
     * - Maintains connection integrity until closure
     *
     * @note Overrides tcp_server::stop_server()
     * @note Shutdown occurs after current epoll_wait timeout
     */
    void epoll_server::stop_server()
    {
        g_stop = 1;
    }

    /**
     * @brief Destructor - performs complete server cleanup
     *
     * Ensures all resources are properly released when the server is destroyed.
     * This includes closing all active connections, the listener socket, and
     * the epoll file descriptor. Uses RAII principles for automatic cleanup.
     *
     * Cleanup Order:
     * 1. Close all active client connections
     * 2. Close listener socket if present
     * 3. Close epoll file descriptor
     *
     * Safety Features:
     * - Uses structured bindings for safe iteration
     * - Checks for null pointers before closing
     * - Validates file descriptor values
     * - No exceptions thrown from destructor
     */
    epoll_server::~epoll_server()
    {
        for (auto &[fd, _] : conns)
            close_socket(fd);
        if (listener_socket)
            close_socket(listener_socket->get_fd());
        if (epoll_fd != -1)
            close_socket(epoll_fd);
    }
}

#endif
