#pragma once

#include <string>
#include <memory>

#include "ip_address.hpp"
#include "family.hpp"
#include "port.hpp"

namespace hh_socket
{
    /**
     * @brief Represents a complete socket address combining IP, port, and address family.
     *
     * This class encapsulates all components needed for network socket operations:
     * IP address, port number, and address family (IPv4/IPv6). It manages the
     * underlying system sockaddr structures and provides type-safe access to
     * socket address components.
     *
     * @note Handles both IPv4 (sockaddr_in) and IPv6 (sockaddr_in6) addresses
     * @note Provides automatic conversion between host and network byte order
     */
    class socket_address
    {
    private:
        /// IP address component
        ip_address address;

        /// Address family (IPv4 or IPv6)
        family family_id;

        /// Port number component
        port port_id;

        /// Underlying system socket address structure
        /// may be either sockaddr_in or sockaddr_in6
        std::shared_ptr<sockaddr> addr;

    public:
        /**
         * @brief Default constructor - creates uninitialized socket address.
         */
        explicit socket_address() = default;

        /**
         * @brief Construct socket address from components.
         * @param port_id Port number
         * @param address IP address default 0.0.0.0
         * @param family_id Address family (IPv4/IPv6) default AF_INET (IPv4)
         *
         * Creates a socket address by combining IP address, port, and family.
         * Automatically creates appropriate sockaddr structure internally.
         */
        explicit socket_address(const port &port_id, const ip_address &address = ip_address("0.0.0.0"), const family &family_id = family(IPV4));

        /**
         * @brief Construct from system sockaddr_storage structure.
         * @param addr Socket address storage structure
         *
         * Creates socket address object from existing system address structure.
         * Extracts IP, port, and family information from the structure.
         */
        explicit socket_address(sockaddr_storage &addr);

        /**
         * @brief Custom copy constructor.
         * @param other Socket address to copy from
         *
         * Creates deep copy of socket address including underlying sockaddr structure.
         */
        socket_address(const socket_address &other);

        /**
         * @brief Custom copy assignment operator.
         * @param other Socket address to copy from
         * @return Reference to this object
         *
         * Assigns socket address components and recreates sockaddr structure.
         */
        socket_address &operator=(const socket_address &other);

        // Move operations
        socket_address(socket_address &&) = default;
        socket_address &operator=(socket_address &&) = default;

        /// Default destructor
        ~socket_address() = default;

        /**
         * @brief Get the IP address component.
         * @return IP address object
         */
        ip_address get_ip_address() const { return address; }

        /**
         * @brief Get the port component.
         * @return Port object
         */
        port get_port() const { return port_id; }

        /**
         * @brief Get the address family component.
         * @return Address family object
         */
        family get_family() const { return family_id; }

        std::string to_string() const
        {
            return address.get() + ":" + std::to_string(port_id.get());
        }

        /**
         * @brief Get raw sockaddr pointer for system calls.
         * @return Pointer to underlying sockaddr structure
         *
         * Returns pointer suitable for use with socket system calls like
         * bind(), connect(), accept(), etc.
         */
        sockaddr *get_sock_addr() const;

        /**
         * @brief Get size of sockaddr structure.
         * @return Size in bytes of the sockaddr structure
         *
         * Returns appropriate size for IPv4 (sockaddr_in) or IPv6 (sockaddr_in6).
         */
        socklen_t get_sock_addr_len() const;

        /// Allow helper functions to access private members
        friend void handle_ipv4(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id);
        friend void handle_ipv6(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id);

        /**
         * @brief Stream insertion operator for output.
         * @param os Output stream
         * @param sa Socket address object to output
         * @return Reference to output stream
         */
        friend std::ostream &operator<<(std::ostream &os, const socket_address &sa)
        {
            os << "IP Address: " << sa.get_ip_address() << ", Port: " << sa.get_port() << ", Family: " << sa.get_family();
            return os;
        }
    };

    /**
     * @brief Helper function to handle IPv4 address initialization.
     * @param addr Socket address object to initialize
     * @param address IP address component
     * @param port_id Port component
     * @param family_id Address family component
     *
     * Creates and initializes sockaddr_in structure for IPv4 addresses.
     */
    void handle_ipv4(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id);

    /**
     * @brief Helper function to handle IPv6 address initialization.
     * @param addr Socket address object to initialize
     * @param address IP address component
     * @param port_id Port component
     * @param family_id Address family component
     *
     * Creates and initializes sockaddr_in6 structure for IPv6 addresses.
     */
    void handle_ipv6(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id);
}