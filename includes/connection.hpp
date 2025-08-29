#pragma once

#include "file_descriptor.hpp"
#include "utilities.hpp"
#include "socket_address.hpp"
#include "data_buffer.hpp"

namespace hh_socket
{
    /**
     * @brief Represents a connection to a remote socket.
     * This class provides an interface for sending and receiving data
     * over the established TCP connection.
     */
    class connection
    {
    private:
        /// File descriptor for the socket
        file_descriptor fd;

        /// Address of the local socket
        socket_address local_addr;

        /// Address of the remote socket
        socket_address remote_addr;

        /// flag to indicate if the connection is open
        bool is_open = true;

    public:
        /**
         * @brief Construct a new connection object
         *
         * @param fd File descriptor representing the socket
         * @param local_addr Address of the local socket
         * @param remote_addr Address of the remote socket
         *
         * @throw socket_exception if the file descriptor is invalid
         */
        connection(file_descriptor fd, const socket_address &local_addr, const socket_address &remote_addr);
        ~connection();

        // Deleted copy constructor and assignment operator
        connection(const connection &) = delete;
        connection &operator=(const connection &) = delete;

        // Move constructor and assignment operator
        connection(connection &&other) noexcept
        {
            fd = std::move(other.fd);
            local_addr = other.local_addr;
            remote_addr = other.remote_addr;
            is_open = other.is_open;
            other.is_open = false;

            other.fd.invalidate();
        }
        connection &operator=(connection &&other) noexcept
        {
            if (this != &other)
            {
                fd = std::move(other.fd);
                local_addr = other.local_addr;
                remote_addr = other.remote_addr;
                is_open = other.is_open;
                other.is_open = false;

                other.fd.invalidate();
            }
            return *this;
        }

        /**
         * @brief Get the file descriptor raw value
         * @return the value of the file descriptor
         */
        int get_fd() const
        {
            return fd.get();
        }

        /**
         * @brief Send data on established connection
         * @param data Data buffer to send
         * @throws socket_exception with type "ProtocolMismatch" if called on non-TCP socket
         * @throws socket_exception with type "SocketWrite" if write operation fails
         * @throws socket_exception with type "PartialWrite" if not all data was sent
         * @return Number of sent bytes
         */
        std::size_t send(const data_buffer &data);

        /**
         * @brief Receive data from established connection
         * @return Buffer containing received data
         * @throws socket_exception with type "ProtocolMismatch" if called on non-TCP socket
         * @throws socket_exception with type "SocketRead" if read operation fails
         */
        data_buffer receive();

        /**
         * @brief Close the connection
         *
         * This function will close the socket and release any resources
         * associated with the connection.
         */
        void close();

        /**
         * @brief Check if the connection is open
         * @return true if the connection is open, false otherwise
         */
        bool is_connection_open() const;

        /**
         * @brief Get the remote address object
         * @return socket_address
         */
        socket_address get_remote_address() const { return remote_addr; }

        /**
         * @brief Get the local address object
         * @return socket_address
         */
        socket_address get_local_address() const { return local_addr; }
    };
}