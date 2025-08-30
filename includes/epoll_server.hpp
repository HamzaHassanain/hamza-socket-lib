#pragma once

/**
 * @file epoll_server.hpp
 * @brief High-performance Linux epoll-based TCP server implementation
 *
 * This file provides a scalable, event-driven TCP server using Linux's epoll
 * mechanism for handling thousands of concurrent connections efficiently.
 * The implementation uses edge-triggered epoll for maximum performance.
 *
 * Platform Support:
 * - Linux only (requires epoll system call)
 * - Automatically disabled on non-Linux platforms
 * - If on Windows, We use a library I found, called wepoll, it is not that performant, but it does the job
 *
 * Features:
 * - Edge-triggered epoll for high performance
 * - Non-blocking I/O operations
 * - Automatic connection management
 * - Configurable file descriptor limits
 * - Thread-safe connection handling
 * - Graceful shutdown support
 *
 * @note This implementation is Linux-specific and will not compile on other platforms
 */

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#include <signal.h>
// check if we are on linux and platform that supports epoll
#if (defined(__linux__) || defined(__linux))
#include <sys/epoll.h>
#include <sys/resource.h>
#else
#include "wepoll.hpp"
#endif

#include "tcp_server.hpp"
#include "socket.hpp"
#include "connection.hpp"
#include "data_buffer.hpp"

/// Custom epoll event used to signal connection closure
const unsigned int HAMZA_CUSTOM_CLOSE_EVENT = 3545940;

namespace hh_socket
{
    /**
     * @brief Connection state structure for epoll-managed connections
     *
     * This structure maintains the state for each active connection in the epoll server.
     * It includes the connection object, output queue for pending writes, and flags
     * to track the connection's I/O state.
     */
    struct epoll_connection
    {
        /// Shared pointer to the connection object
        std::shared_ptr<connection> conn;

        /// Queue of pending outbound messages waiting to be sent
        std::deque<std::string> outq; // queued writes

        /// Flag indicating if the connection wants to write (EPOLLOUT enabled)
        bool want_write = false;

        /// Flag indicating if the connection wants to close, meant to be set by user
        bool want_close = false;
    };

    /**
     * @brief  Linux epoll-based TCP server
     *
     * This class implements a scalable TCP server using Linux's epoll mechanism
     * for efficient event-driven I/O. It can handle thousands of concurrent
     * connections with minimal system resources by using edge-triggered epoll
     * and non-blocking sockets.
     *
     * Key Features:
     * - Edge-triggered epoll for maximum performance
     * - Non-blocking accept loop for handling connection bursts
     * - Automatic write buffering and flow control
     * - Configurable file descriptor limits via setrlimit
     * - Thread-safe connection management
     *
     * Architecture:
     * - Single-threaded event loop (epoll_wait)
     * - Non-blocking I/O operations throughout
     * - Connection state tracking via epoll_connection struct
     * - Automatic cleanup of closed connections
     *
     * Performance Characteristics:
     * - O(1) event notification via epoll
     * - Scales to thousands of concurrent connections
     * - Minimal memory overhead per connection
     * - Efficient handling of partial reads/writes

     * @note This class is Linux-specific and requires kernel epoll support
     * @note Implements tcp_server interface
     */
    class epoll_server : public tcp_server
    {
    private:
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        HANDLE epoll_fd = INVALID_HANDLE_VALUE;
#else
        /// Epoll file descriptor for event monitoring
        int epoll_fd = -1;
#endif

        /// Shared pointer to the listening socket
        std::shared_ptr<socket> listener_socket;

        /// Vector of epoll events for batch event processing
        std::vector<epoll_event> events;

        /// Atomic flag for graceful shutdown signaling
        volatile sig_atomic_t g_stop = 0;

        /// Current number of open connections
        std::size_t current_open_connections = 10;

        /// Maximum number of file descriptors, if failed setting to the specified max
        std::size_t max_fds = 1024;

        /// @brief  tries to accept connections
        void try_accept();

        /// @brief  Tries to read data from a connection
        /// @param c Reference to the epoll_connection to read from
        void try_read(epoll_connection &c);

#if (defined(__linux__) || defined(__linux))
        /**
         * @brief Sets file descriptor limit for the process
         * @param soft Soft limit for file descriptors
         * @param hard Hard limit for file descriptors
         * @return 0 on success, -1 on failure
         *
         * Configures the maximum number of file descriptors the process can open.
         * Essential for servers handling many concurrent connections.
         */
        int set_rlimit_nofile(rlim_t soft, rlim_t hard);
#endif
        /**
         * @brief Adds file descriptor to epoll monitoring
         * @param fd File descriptor to monitor
         * @param ev Epoll events to monitor (EPOLLIN, EPOLLOUT, etc.)
         * @return 0 on success, -1 on failure
         *
         * Registers a file descriptor with the epoll instance for event monitoring.
         * Uses edge-triggered mode (EPOLLET) for maximum performance.
         */
        int add_epoll(int fd, uint32_t ev);

        /**
         * @brief Modifies epoll monitoring events for a file descriptor
         * @param fd File descriptor to modify
         * @param ev New epoll events to monitor
         * @return 0 on success, -1 on failure
         *
         * Updates the events being monitored for an existing file descriptor.
         * Used to enable/disable write monitoring based on output queue state.
         */
        int mod_epoll(int fd, uint32_t ev);

        /**
         * @brief Removes file descriptor from epoll monitoring
         * @param fd File descriptor to remove
         * @return 0 on success, -1 on failure
         *
         * Unregisters a file descriptor from epoll monitoring.
         * Called during connection cleanup.
         */
        int del_epoll(int fd);

        /**
         * @brief Closes and cleans up a connection
         * @param fd File descriptor of the connection to close
         *
         * Performs complete cleanup of a connection:
         * - Removes from epoll monitoring
         * - Calls connection closed callback
         * - Closes the socket
         * - Removes from connection map
         */
        void close_conn(int fd);

        /**
         * @brief Attempts to flush pending writes for a connection
         * @param c Reference to the epoll_connection to flush
         * @return true if all data was sent, false if more data remains
         *
         * Tries to send all queued data for a connection. Handles partial sends
         * gracefully by updating the queue. Returns false if EAGAIN/EWOULDBLOCK
         * is encountered, indicating the socket buffer is full.
         */
        bool flush_writes(epoll_connection &c);

        /**
         * @brief Main event loop using epoll_wait
         * @param timeout Timeout in milliseconds for epoll_wait (-1 for blocking)
         *
         * The core of the server - runs the event loop that:
         * - Waits for events using epoll_wait
         * - Handles new connections (accept loop)
         * - Processes incoming data
         * - Manages outgoing data and flow control
         * - Handles connection closures
         * - Manages epoll event buffer resizing
         */
        void epoll_loop(int timeout = 1000);

    protected:
        /// Map of file descriptors to their connection state
        std::unordered_map<int, epoll_connection> conns;

        /**
         * @brief Interface for derived classes to close a connection
         * @param conn Shared pointer to the connection to close
         *
         * Provides a clean interface for derived classes to request connection closure.
         * Uses custom epoll event to signal the main loop to close the connection
         * during the next event processing cycle.
         *
         * @note Never close the connection directly from outside, nor use conn->close()
         * @note Overrides tcp_server::close_connection
         */
        void close_connection(std::shared_ptr<connection> conn) override;

        /**
         * @brief Stops reading from a connection (disables EPOLLIN)
         * @note it just sets want to stop to be true
         * @param conn Shared pointer to the connection to stop reading from
         */
        void stop_reading_from_connection(std::shared_ptr<connection> conn);

        /**
         * @brief Closes a connection identified by its file descriptor
         *
         * @param fd File descriptor of the connection to close
         */
        void close_connection(int fd);

        /**
         * @brief Interface for derived classes to send messages
         * @param conn Shared pointer to the target connection
         * @param db Data buffer containing the message to send
         *
         * Queues a message for sending to the specified connection. The message
         * is added to the connection's output queue and epoll is notified to
         * monitor for write availability.
         *
         * @note Never Use conn->send(), always use send_message()
         * @note Overrides tcp_server::send_message
         * @note Messages are sent asynchronously when the socket is ready
         */
        void send_message(std::shared_ptr<connection> conn, const data_buffer &db) override;

        /**
         * @brief Called when an exception occurs during server operation
         * @param e The exception that occurred
         *
         * Default implementation logs the exception to stderr. Derived classes
         * can override this to implement custom error handling, logging, or
         * recovery mechanisms.
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_exception_occurred(const std::exception &e) override;

        /**
         * @brief Called when a new client connection is established
         * @param conn Shared pointer to the newly connected client
         *
         * Default implementation logs connection information. Derived classes
         * can override this to implement custom connection handling, authentication,
         * or initialization logic.
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_connection_opened(std::shared_ptr<connection> conn) override;

        /**
         * @brief Called when a client connection is closed
         * @param conn Shared pointer to the closed connection
         *
         * Default implementation logs disconnection information. Derived classes
         * can override this to implement custom cleanup, logging, or resource
         * management for closed connections.
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_connection_closed(std::shared_ptr<connection> conn) override;

        /**
         * @brief Called when data is received from a client
         * @param conn Shared pointer to the connection that sent data
         * @param db Data buffer containing the received data
         *
         * Default implementation spawns a detached thread to handle the message,
         * providing an echo server functionality. Derived classes should override
         * this to implement custom message processing logic.
         *
         * @note Virtual function - should be overridden by derived classes
         * @note Default implementation uses detached threads - consider thread pool for production
         */
        virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override;

        /**
         * @brief Called when the server successfully starts listening
         *
         * Default implementation logs the listening socket information.
         * Derived classes can override this to implement custom startup logic.
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_listen_success() override;

        /**
         * @brief Called when the server shuts down successfully
         *
         * Default implementation logs shutdown completion. Derived classes
         * can override this to implement custom shutdown procedures or cleanup.
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_shutdown_success() override;

        /**
         * @brief Called when the server is waiting for activity
         *
         * Default implementation logs the waiting state. Derived classes
         * can override this to implement custom waiting behavior .
         *
         * @note Virtual function - can be overridden by derived classes
         */
        virtual void on_waiting_for_activity() override
        {
        }

    public:
        /**
         * @brief Constructs an epoll server with specified file descriptor limit
         * @param max_fds Maximum number of file descriptors the server can handle
         *
         * Initializes the epoll server by:
         * - Setting process file descriptor limits via setrlimit
         * - Creating epoll instance with EPOLL_CLOEXEC flag
         * - Allocating initial event buffer (4096 events)
         * - Validating epoll creation success
         *
         * @throws std::runtime_error if epoll creation fails
         * @note Higher max_fds allows more concurrent connections but uses more memory
         */
        epoll_server(int max_fds);

        /**
         * @brief Virtual destructor for proper cleanup
         *
         * Ensures proper cleanup of all resources:
         * - Closes all active connections
         * - Closes listener socket if present
         * - Closes epoll file descriptor
         *
         * @note Virtual destructor allows proper cleanup in inheritance hierarchies
         */
        virtual ~epoll_server();

        /**
         * @brief Starts the server event loop
         * @param timeout Timeout in milliseconds for epoll_wait calls
         *
         * Starts the main event loop that processes:
         * - New incoming connections
         * - Data ready for reading
         * - Sockets ready for writing
         * - Connection errors and closures
         * - Graceful shutdown signals
         *
         * This method blocks until the server is stopped via stop_server()
         * or a signal is received.
         *
         * @note Overrides tcp_server::listen
         * @note This method blocks until server shutdown
         */
        virtual void listen(int timeout) override;

        /**
         * @brief Registers a listening socket with the epoll server
         * @param sock_ptr Shared pointer to the configured listening socket
         * @return true if registration successful, false otherwise
         *
         * Registers a pre-configured listening socket with the epoll instance.
         * The socket should already be bound and in listening state before
         * calling this method.
         *
         * Requirements for the socket:
         * - Must be bound to an address
         * - Must be in listening state (listen() called)
         * - Should be configured with desired socket options
         *
         * @note The socket should be configured as non-blocking for optimal performance
         * @note Only one listening socket is supported per server instance
         * @note use make_listener_socket(....) to create a properly configured listening socket
         */
        virtual bool register_listener_socket(std::shared_ptr<socket> sock_ptr);

        /**
         * @brief Signals the server to stop gracefully
         *
         * Sets the stop flag that will cause the event loop to exit cleanly
         * after processing current events.
         *
         * @note Overrides tcp_server::stop_server
         * @note The server will stop after the current epoll_wait timeout expires
         */
        virtual void stop_server() override;
    };
}
