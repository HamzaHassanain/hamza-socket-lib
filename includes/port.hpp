#pragma once

#include <stdexcept>
#include <ostream>
#include "utilities.hpp"
#include "exceptions.hpp"
namespace hh_socket
{
    /**
     * @brief Represents a network port number with validation.
     *
     * This class provides type-safe wrapper around port numbers, ensuring
     * only valid port values (0-65535) are used. It prevents common networking
     * errors by validating port numbers at construction time.
     *
     * @note Uses explicit constructors to prevent implicit conversions.
     * @note The class does not check if the port number is already in use, there is a separate mechanism for that.
     */
    class port
    {
    private:
        /// Port number (0-65535)
        int port_id;

        /**
         * @brief Validates and sets the port ID.
         * @param id Port number to validate and set
         * @throws hh_socket::invalid_port_exception if port is not in range 0-65535
         */
        void set_port_id(int id)
        {
            if (id < hh_socket::MIN_PORT || id > hh_socket::MAX_PORT)
            {
                throw socket_exception("Port number must be in range 0-65535", "InvalidPort", __func__);
            }
            port_id = id;
        }

    public:
        /**
         * @brief Default constructor - creates uninitialized port.
         *
         * Creates a port object with undefined value. Must be assigned
         * a valid port number before use.
         */
        explicit port() = default;

        /**
         * @brief Construct port with validation.
         * @param id Port number to use
         * @throws std::invalid_argument if port is not in valid range
         */
        explicit port(int id) { set_port_id(id); }

        // Copy and move operations
        port(const port &) = default;
        port(port &&) = default;
        port &operator=(const port &) = default;
        port &operator=(port &&) = default;

        /**
         * @brief Get the port number.
         * @return Port number as integer
         */
        int get() const { return port_id; }

        /**
         * @brief Equality comparison operator.
         * @param other Port to compare with
         * @return true if port numbers are equal
         */
        bool operator==(const port &other) const
        {
            return port_id == other.port_id;
        }

        /**
         * @brief Inequality comparison operator.
         * @param other Port to compare with
         * @return true if port numbers are different
         */
        bool operator!=(const port &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Less-than comparison for ordering.
         * @param other Port to compare with
         * @return true if this port number is less than other's
         */
        bool operator<(const port &other) const
        {
            return port_id < other.port_id;
        }

        /**
         * @brief Stream insertion operator.
         * @param os Output stream
         * @param p Port object to output
         * @return Reference to output stream
         */
        friend std::ostream &operator<<(std::ostream &os, const port &p)
        {
            os << p.port_id;
            return os;
        }

        /// Default destructor
        ~port() = default;
    };
}