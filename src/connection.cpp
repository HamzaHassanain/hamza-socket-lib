#include "../includes/connection.hpp"
#include "../includes/utilities.hpp"
namespace hh_socket
{

    connection::connection(file_descriptor fd, const socket_address &local_addr, const socket_address &remote_addr)
        : fd(std::move(fd)), local_addr(local_addr), remote_addr(remote_addr), is_open(true)
    {
        if (this->fd.get() == INVALID_SOCKET_VALUE || this->fd.get() == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Invalid file descriptor", "ConnectionCreation", __func__);
        }
    }

    /**
     * Sends data over established TCP connection.
     * Uses ::write() system call in loop to ensure all data is transmitted.
     * TCP may send data in multiple chunks, so loop until all data sent.
     * Tracks total bytes sent to detect partial transmission issues.
     */
    std::size_t connection::send(const data_buffer &data)
    {
        if (!is_open || fd.get() == SOCKET_ERROR_VALUE || fd.get() == INVALID_SOCKET_VALUE)
        {
            return 0;
        }
        auto bytes_sent = ::send(fd.get(), data.data(), data.size(), 0);
        if (bytes_sent == SOCKET_ERROR_VALUE)
        {
            throw socket_exception("Failed to write data for fd:  " + std::to_string(fd.get()) + " " + std::string(get_error_message()), "SocketWrite", __func__);
        }
        return bytes_sent;
    }

    /**
     * Receives data from established TCP connection.
     * Uses ::recv() system call in loop to receive all available data.
     * Continues reading until no more data available or connection closed.
     * Handles non-blocking sockets by checking for EAGAIN/EWOULDBLOCK.
     */
    data_buffer connection::receive()
    {
        if (!is_open || fd.get() == SOCKET_ERROR_VALUE || fd.get() == INVALID_SOCKET_VALUE)
        {
            return data_buffer();
        }

        data_buffer received_data;
        char buffer[MAX_BUFFER_SIZE];

        int bytes_received = ::recv(fd.get(), buffer, sizeof(buffer), 0);

        /// EOF
        if (bytes_received == 0)
        {
            return data_buffer();
        }
        if (bytes_received == SOCKET_ERROR_VALUE)
        {
/*
EAGAIN or EWOULDBLOCK: The socket is non-blocking, and no data is currently available.
ENOTCONN: The socket is not connected.

@throw error for the follwing return values
ECONNRESET: The connection was forcibly closed by the peer.
EINTR: The function call was interrupted by a signal.
*/
#if defined(SOCKET_PLATFORM_UNIX)
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return data_buffer();
            }
#elif defined(SOCKET_PLATFORM_WINDOWS)
            if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR)
            {
                return data_buffer();
            }
#endif
            throw socket_exception("Failed to read data for fd " + std::to_string(fd.get()) + " " + std::string(get_error_message()), "SocketRead", __func__);
        }

        received_data.append(buffer, bytes_received);
        return received_data;
    }

    void connection::close()
    {
        if (is_open)
        {
            is_open = false;
            close_socket(fd.get());
            fd.invalidate();
        }
    }

    bool connection::is_connection_open() const
    {
        return is_open;
    }

    connection::~connection()
    {
        close();
    }
};
