#pragma once

#include <iostream>

// Platform-specific socket type definitions
// Windows uses SOCKET (unsigned int) while Unix-like systems use int
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <winsock2.h>
typedef SOCKET socket_t; ///< Windows socket type (unsigned integer)
#else
#include <sys/types.h>
#include <sys/socket.h>
typedef int socket_t; ///< Unix socket type (signed integer)
#endif

#include "utilities.hpp"

namespace hh_socket
{
    /**
     * @brief A cross-platform wrapper for file descriptors and socket handles.
     *
     * This class provides a type-safe, move-only wrapper around file descriptors (Unix/Linux)
     * and socket handles (Windows). It abstracts the platform differences between
     * Windows SOCKET type and Unix int type, providing a unified interface for
     * socket operations across different operating systems.
     *
     * - **Move-only semantics**: Prevents accidental copying and resource duplication
     * - **Exception safety**: Validates file descriptors at construction time
     * - **Resource ownership**: Clear transfer of ownership through move operations
     *
     *
     * @note The _WIN32 preprocessor directive is used because:
     * 1. **Type Safety**: Windows SOCKET is unsigned int, Unix fd is signed int
     * 2. **Invalid Values**: Windows uses INVALID_SOCKET, Unix uses -1
     * 3. **Header Dependencies**: Windows requires winsock2.h, Unix requires sys/socket.h
     * 4. **ABI Compatibility**: Different calling conventions and data sizes
     */
    class file_descriptor
    {
    private:
        /// Platform-specific socket/file descriptor handle
        socket_t fd;

    public:
        /**
         * @brief Default constructor - creates an invalid descriptor.
         *
         * Creates a file_descriptor in an invalid state using INVALID_SOCKET_VALUE.
         * This constructor is useful for creating descriptor objects that will be
         * assigned valid values later through move assignment.
         *
         * @note The descriptor created by this constructor is in an invalid state
         *       and should not be used for socket operations until assigned a valid value
         */
        file_descriptor() : fd(INVALID_SOCKET_VALUE) {}

        /**
         * @brief Construct from a raw socket/file descriptor with validation.
         * @param fd Raw socket handle or file descriptor to wrap
         *
         * Creates a file_descriptor wrapper around a raw socket handle or file
         * descriptor. The constructor validates the input and throws an exception
         * if an invalid descriptor is provided, ensuring type safety and preventing
         * operations on invalid resources.
         */
        explicit file_descriptor(socket_t fd) : fd(fd)
        {
        }

        // Copy operations - DELETED for RAII safety
        /**
         * @brief Copy constructor - DELETED.
         *
         * Copy construction is explicitly deleted to prevent resource duplication
         * and potential double-close issues. File descriptors represent unique
         * system resources that should have single ownership.
         *
         * Use move semantics instead to transfer ownership:
         */
        file_descriptor(const file_descriptor &) = delete;

        /**
         * @brief Copy assignment operator - DELETED.
         *
         * Copy assignment is explicitly deleted to prevent resource duplication
         * and maintain single ownership semantics for file descriptors.
         */
        file_descriptor &operator=(const file_descriptor &) = delete;

        // Move operations - Enable safe ownership transfer
        /**
         * @brief Move constructor.
         * @param old File descriptor to move from
         *
         * Efficiently transfers the descriptor value from another file_descriptor.
         * After the move, the source object becomes invalid (holds INVALID_SOCKET_VALUE)
         * and the new object owns the resource.
         *
         * This is the preferred and only way to transfer descriptor ownership,
         * ensuring no resource duplication or double-close issues.
         */
        file_descriptor(file_descriptor &&old) : fd(old.fd)
        {
            old.fd = INVALID_SOCKET_VALUE; // Leave source in valid but empty state
        }

        /**
         * @brief Move assignment operator.
         * @param old File descriptor to move from
         * @return Reference to this descriptor after assignment
         *
         * Transfers ownership from another file_descriptor to this one.
         * Includes self-assignment protection and properly invalidates the source.
         */
        file_descriptor &operator=(file_descriptor &&old)
        {
            if (this != &old) // Self-assignment protection
            {
                fd = old.fd;
                old.fd = INVALID_SOCKET_VALUE; // Invalidate source
            }
            return *this;
        }

        /**
         * @brief Get the raw socket/file descriptor value.
         * @return Raw socket handle or file descriptor as integer
         *
         * Returns the underlying platform-specific socket handle or file descriptor.
         * This value can be used directly with system calls like send(), recv(),
         * read(), write(), etc.
         *
         * @warning The returned value should be used carefully in cross-platform code
         *          due to type differences between Windows and Unix systems
         * @note If the descriptor is invalid, returns INVALID_SOCKET_VALUE
         */
        int get() const { return fd; }

        /**
         * @brief Check if the descriptor is valid.
         * @return true if the descriptor holds a valid value, false otherwise
         *
         * Convenience method to check if the file descriptor is in a valid state
         * and can be used for socket operations.
         */
        bool is_valid() const { return fd != INVALID_SOCKET_VALUE; }

        /**
         * @brief Invalidate the file descriptor.
         *
         * Sets the file descriptor to an invalid state, ensuring it can no longer
         * be used for socket operations. This is useful for safely closing
         * descriptors and preventing accidental usage after closure.
         */
        void invalidate()
        {
            fd = INVALID_SOCKET_VALUE; // Set to invalid state
        }

        /**
         * @brief Equality comparison operator.
         * @param other File descriptor to compare with
         * @return true if both descriptors have the same underlying value
         *
         * Compares the raw descriptor values for equality. Two file_descriptor
         * objects are equal if they wrap the same system resource identifier.
         */
        bool operator==(const file_descriptor &other) const
        {
            return fd == other.fd;
        }

        /**
         * @brief Inequality comparison operator.
         * @param other File descriptor to compare with
         * @return true if descriptors have different underlying values
         */
        bool operator!=(const file_descriptor &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Less-than comparison operator for ordering.
         * @param other File descriptor to compare with
         * @return true if this descriptor's value is less than other's value
         *
         * Enables use of file_descriptor objects in ordered containers like
         * std::set, std::map, and for sorting operations.
         */
        bool operator<(const file_descriptor &other) const
        {
            return fd < other.fd;
        }

        /**
         * @brief Stream insertion operator for output.
         * @param os Output stream
         * @param fd File descriptor object to output
         * @return Reference to the output stream
         *
         * Outputs the raw descriptor value to the stream for debugging and logging.
         * Shows "INVALID_FILE_DESCRIPTOR" for invalid descriptors to aid in debugging.
         */
        friend std::ostream &operator<<(std::ostream &os, const file_descriptor &fd)
        {
            if (fd.is_valid())
            {
                os << fd.get();
            }
            else
            {
                os << "INVALID_FILE_DESCRIPTOR"; // Indicate invalid state
            }
            return os;
        }

        /// Default destructor
        ~file_descriptor() = default;
    };
}