#pragma once

#include <string>
#include <ostream>

namespace hh_socket
{
    /**
     * @brief Represents an IP address (IPv4 or IPv6) for network operations.
     *
     * This class provides a type-safe wrapper around IP address strings, offering
     * validation and convenient comparison operations. It can handle both IPv4
     * addresses (e.g., "192.168.1.1") and IPv6 addresses (e.g., "::1", "2001:db8::1").
     *
     * The class stores the IP address as a string representation, which is
     * platform-independent and suitable for both display and network operations.
     * It provides standard comparison operators for use in containers and sorting.
     *
     * @note Uses explicit constructors to prevent implicit conversions for type safety.
     * @note The class does not perform validation of IP address format - it stores
     *       whatever string is provided. Validation should be performed by the caller
     *       or higher-level network classes.
     *
     * @endcode
     */
    class ip_address
    {
    private:
        /// String representation of the IP address
        std::string address;

    public:
        /**
         * @brief Default constructor - creates an empty IP address.
         *
         * Creates an ip_address object with an empty string. This is useful for
         * creating objects that will be assigned values later or for representing
         * an unspecified address.
         *
         * Marked explicit to prevent implicit conversions.
         */
        explicit ip_address() = default;

        /**
         * @brief Construct IP address from string representation.
         * @param address String containing the IP address
         *
         * Creates an ip_address object from a string representation of an IP address.
         * The string can contain IPv4 addresses (dotted decimal notation) or IPv6
         * addresses (colon-hexadecimal notation).
         *
         * @note This constructor does not validate the IP address format.
         *       Invalid strings will be stored as-is, which may cause issues
         *       in network operations.
         */
        explicit ip_address(const std::string &address) : address(address) {}

        // Copy operations - Safe and efficient for string-based class
        ip_address(const ip_address &) = default;
        ip_address &operator=(const ip_address &) = default;

        // Move operations - Efficient for string transfer
        ip_address(ip_address &&) = default;
        ip_address &operator=(ip_address &&) = default;

        /**
         * @brief Get the IP address string.
         * @return Const reference to the IP address string
         *
         * Returns a const reference to the internal string representation
         * of the IP address. This avoids copying and allows efficient
         * access to the address value.
         */
        const std::string &get() const { return address; }

        /**
         * @brief Equality comparison operator.
         * @param other IP address object to compare with
         * @return true if both objects contain the same IP address string
         */
        bool operator==(const ip_address &other) const
        {
            return address == other.address;
        }

        /**
         * @brief Inequality comparison operator.
         * @param other IP address object to compare with
         * @return true if objects contain different IP address strings
         */
        bool operator!=(const ip_address &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Less-than comparison operator for ordering.
         * @param other IP address object to compare with
         * @return true if this address string is lexicographically less than other's
         */
        bool operator<(const ip_address &other) const
        {
            return address < other.address;
        }

        /**
         * @brief Stream insertion operator for output.
         * @param os Output stream
         * @param ip IP address object to output
         * @return Reference to the output stream
         */
        friend std::ostream &operator<<(std::ostream &os, const ip_address &ip)
        {
            os << ip.address;
            return os;
        }

        /// Default destructor
        ~ip_address() = default;
    };
}