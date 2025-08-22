#pragma once

#include <memory>
#include <socket.hpp>
#include <file_descriptor.hpp>
#include <mutex>

// Platform-specific includes for select() functionality
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace hamza_socket
{
    /**
     * @brief Cross-platform thread-safe wrapper for select() system call I/O multiplexing.
     *
     * This class provides a high-level interface for monitoring multiple file
     * descriptors for read activity using the select() system call. It manages
     * fd_set structures and handles the complexities of select() operations
     * including timeout configuration and thread safety across different platforms.
     *
     * Platform Support:
     * - **Windows**: Uses Winsock select() with SOCKET handles
     * - **Linux/Unix**: Uses POSIX select() with file descriptors
     * - **Cross-platform compatibility**: Abstracts platform differences in headers and function signatures
     *
     * The class maintains a master set of file descriptors and creates working
     * copies for each select() call, as select() modifies the fd_set structures.
     * All operations are thread-safe using mutex protection.
     *
     * Platform Differences Handled:
     * - **Headers**: Windows uses winsock2.h, Unix uses sys/select.h
     * - **select() signature**: Windows ignores first parameter, Unix requires highest fd + 1
     * - **Error handling**: Windows uses WSAGetLastError(), Unix uses errno
     *
     * @note select() is limited to FD_SETSIZE file descriptors (typically 1024 on Unix, 64 on Windows)
     * @note For better scalability, consider migrating to poll() or epoll() (Unix) or I/O Completion Ports (Windows)
     * @note Thread-safe for all operations using internal mutex
     * @note On Windows, requires Winsock to be initialized before use
     */
    class select_server
    {
    private:
        /// Master set of file descriptors to monitor for read activity
        fd_set master_fds;

        /// Working copy of master_fds used in select() calls
        fd_set read_fds;

        /// Maximum number of file descriptors (unused, kept for compatibility)
        int max_fds;

        /// Mutex for thread-safe operations on fd_sets
        std::mutex mtx;

        /// Timeout configuration for select() calls
        int tv_sec;
        int tv_usec;

        struct timeval make_timeval(int seconds, int microseconds)
        {
            struct timeval timeout;
            timeout.tv_sec = seconds;
            timeout.tv_usec = microseconds;
            return timeout;
        }

    public:
        /**
         * @brief Initialize select server with initial file descriptor.
         * @param FD Initial file descriptor to monitor (typically server socket)
         * @note Clears all fd_sets and adds the initial file descriptor
         * @note Sets max_fd to the provided file descriptor value
         */
        void init(const int &FD);

        /**
         * @brief Set the highest file descriptor number for select optimization.
         * @param max_fd Highest file descriptor number currently being monitored
         * @note Thread-safe operation using internal mutex
         * @note select() requires knowing the highest fd + 1 for efficiency
         */
        // void set_max_fd(int max_fd);

        /**
         * @brief Configure timeout for select() operations.
         * @param seconds Timeout duration in seconds (0 for non-blocking)
         * @param microseconds Timeout duration in microseconds (0 for whole seconds)
         * @note Sets microseconds component to 0
         * @note Timeout applies to all subsequent select() calls
         */
        void set_timeout(int seconds = 1, int microseconds = 0);

        /**
         * @brief Remove file descriptor from monitoring set.
         * @param FD File descriptor to stop monitoring
         * @note Thread-safe operation using internal mutex
         * @note Safe to call even if FD is not in the set
         */
        void remove_fd(const int &FD);

        /**
         * @brief Add file descriptor to monitoring set.
         * @param fd File descriptor to start monitoring for read activity
         * @note Thread-safe operation using internal mutex
         * @note File descriptor must be valid and open
         */
        void add_fd(const int &fd);

        /**
         * @brief Check if file descriptor has pending read activity.
         * @param fd File descriptor to check
         * @return true if file descriptor has data ready to read, false otherwise
         * @note Must be called after select() returns > 0
         * @note Thread-safe operation using internal mutex
         */
        bool is_fd_set(const int &fd);

        /**
         * @brief Execute select() system call and wait for activity.
         * @return Number of file descriptors ready for I/O, 0 on timeout, -1 on error
         * @note Thread-safe operation using internal mutex
         * @note Copies master_fds to read_fds before calling select()
         * @note Uses configured timeout for blocking behavior
         * @note Cross-platform: Windows ignores first parameter, Unix uses it for optimization
         */
        int select();
    };
};