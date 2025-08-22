#include <select_server.hpp>

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
     * Initialize select server with initial file descriptor.
     * Clears both master and working fd_sets to ensure clean state.
     * Adds the initial file descriptor (typically server socket) to monitoring set.
     * Sets maximum file descriptor for select() optimization.
     */
    void select_server::init(const int &FD)
    {
        // Clear all bits in master file descriptor set
        // FD_ZERO initializes fd_set to empty state (all bits = 0)
        FD_ZERO(&master_fds);

        // Clear working copy used for select() calls
        // select() modifies this set, so we need a clean copy each time
        FD_ZERO(&read_fds);

        // Add initial file descriptor to master set
        // FD_SET sets the bit corresponding to the file descriptor
        // This file descriptor will be monitored for read activity
        FD_SET(FD, &master_fds);

        // // Set maximum file descriptor for select() efficiency
        // // select() scans from 0 to max_fd, so keeping this accurate improves performance
        // set_max_fd(FD);
    }

    /**
     * Configure timeout for select() operations.
     * Sets seconds component and zeros microseconds for whole-second timeouts.
     * Timeout affects blocking behavior of subsequent select() calls.
     */
    void select_server::set_timeout(int seconds, int microseconds)
    {
        // Set seconds component of timeout structure
        tv_sec = seconds;

        // Set microseconds component to 0 (no fractional seconds)
        // For more precise timing, this could be configurable
        tv_usec = microseconds;
    }

    /**
     * Add file descriptor to monitoring set in thread-safe manner.
     * Uses mutex to prevent race conditions when multiple threads modify fd_sets.
     * File descriptor will be monitored for read activity in subsequent select() calls.
     */
    void select_server::add_fd(const int &fd)
    {
        // Acquire exclusive lock for thread safety
        // RAII pattern ensures automatic unlock when scope exits
        std::lock_guard<std::mutex> lock(mtx);

        // Add file descriptor to master monitoring set
        // FD_SET is a macro that sets the appropriate bit in the fd_set
        FD_SET(fd, &master_fds);
    }

    /**
     * Remove file descriptor from monitoring set in thread-safe manner.
     * Uses mutex to prevent race conditions during fd_set modifications.
     * Safe to call even if file descriptor is not currently in the set.
     */
    void select_server::remove_fd(const int &fd)
    {
        // Acquire exclusive lock for thread safety
        std::lock_guard<std::mutex> lock(mtx);

        // Remove file descriptor from master monitoring set
        // FD_CLR clears the bit corresponding to the file descriptor
        // If fd is not set, this operation has no effect
        FD_CLR(fd, &master_fds);
    }

    /**
     * Execute select() system call and wait for I/O activity.
     * Creates working copy of master fd_set since select() modifies the set.
     * Returns number of ready file descriptors or error conditions.
     * Thread-safe operation using mutex protection.
     */
    int select_server::select()
    {
        // Acquire exclusive lock for thread safety
        std::lock_guard<std::mutex> lock(mtx);

        // Copy master_fds to read_fds for select() call
        // select() modifies the fd_set to indicate which descriptors are ready
        // We preserve master_fds for future calls
        read_fds = master_fds;

        auto timeout = make_timeval(tv_sec, tv_usec);

        // Call select() system call with cross-platform considerations:
        // Windows: First parameter is ignored, can pass 0
        // Unix/Linux: First parameter should be highest fd + 1
        //
        // Parameters:
        // - next_available_fd: next available file descriptor to monitor (ignored on Windows)
        // - &read_fds: set to monitor for read activity
        // - nullptr: not monitoring write activity
        // - nullptr: not monitoring exceptional conditions
        // - &timeout: timeout configuration
        //
        // Returns:
        // - > 0: number of file descriptors ready for I/O
        // - 0: timeout occurred, no activity
        // - < 0: error occurred (check errno on Unix, WSAGetLastError() on Windows)
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        // Windows: First parameter is ignored, can pass 0
        return ::select(0, &read_fds, nullptr, nullptr, &timeout);
#else
        // Unix/Linux: First parameter should be highest fd + 1 for efficiency
        return ::select(2048, &read_fds, nullptr, nullptr, &timeout);
#endif
    }

    /**
     * Set maximum file descriptor for select() optimization.
     * select() scans file descriptors from 0 to max_fd, so accuracy improves performance.
     * Thread-safe operation to prevent race conditions during updates.
     */
    // void select_server::set_max_fd(int max_fd)
    // {
    //     // Acquire exclusive lock for thread safety
    //     std::lock_guard<std::mutex> lock(mtx);

    //     // Update maximum file descriptor value
    //     // This should be the highest file descriptor currently being monitored
    // }

    /**
     * Check if specific file descriptor has pending read activity.
     * Must be called after select() returns a positive value.
     * Uses the read_fds set that was modified by the most recent select() call.
     */
    bool select_server::is_fd_set(const int &fd)
    {
        // Acquire exclusive lock for thread safety
        std::lock_guard<std::mutex> lock(mtx);

        // Check if file descriptor bit is set in read_fds
        // FD_ISSET returns non-zero if the bit is set, 0 otherwise
        // This indicates whether the file descriptor has data ready to read
        return FD_ISSET(fd, &read_fds);
    }
};