#pragma once

// check if we are on linux  and platfrom that supports epoll
#if defined(__linux__) || defined(__linux)

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>

#include <tcp_server.hpp>
#include <socket.hpp>
#include <port.hpp>
#include <socket_address.hpp>
#include <utilities.hpp>
#include <connection.hpp>

const u_int32_t HAMZA_CUSTOM_CLOSE_EVENT = 3545940;
namespace hamza_socket
{
    struct epoll_connection
    {
        std::shared_ptr<connection> conn;
        std::deque<std::string> outq; // queued writes
        bool want_write = false;
    };
    class epoll_server : public tcp_server
    {

        int epoll_fd = -1;
        std::shared_ptr<socket> listener_socket;
        std::unordered_map<int, epoll_connection> conns;
        std::vector<epoll_event> events;

        volatile sig_atomic_t g_stop = 0;
        void on_sigint(int) { g_stop = 1; }

        int set_rlimit_nofile(rlim_t soft, rlim_t hard)
        {
            struct rlimit rl{soft, hard};
            return setrlimit(RLIMIT_NOFILE, &rl);
        }
        int add_epoll(int fd, uint32_t ev)
        {
            epoll_event e{};
            e.events = ev;
            e.data.fd = fd;
            return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e);
        }
        int mod_epoll(int fd, uint32_t ev)
        {
            epoll_event e{};
            e.events = ev;
            e.data.fd = fd;
            // std::cout << fd << " " << ev << std::endl;

            return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &e);
        }
        int del_epoll(int fd)
        {
            return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        }
        void close_conn(int fd)
        {
            del_epoll(fd);
            on_connection_closed(conns[fd].conn);
            close_socket(fd);
            conns.erase(fd);
        }
        bool flush_writes(epoll_connection &c)
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
                        // Cannot write more now
                        return false;
                    }
                    // Error
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
        void epoll_loop(int timeout = 1000)
        {
            on_listen_success();
            while (!g_stop)
            {

                int n = epoll_wait(epoll_fd, events.data(), (int)events.size(), timeout);
                if (n < 0)
                {
                    if (errno == EINTR)
                        continue;
                    // Fatal error
                    on_exception_occurred(std::runtime_error("epoll_wait failed: " + std::string(strerror(errno))));
                    break;
                }
                if (n == (int)events.size())
                {
                    // grow event buffer if saturated
                    events.resize(events.size() * 2);
                }

                for (int i = 0; i < n; ++i)
                {
                    uint32_t ev = events[i].events;
                    int fd = events[i].data.fd;

                    // Server Accepting Connections
                    if (listener_socket && fd == listener_socket->get_fd())
                    {
                        // Accept as many as possible (edge-triggered accept loop)

                        while (true)
                        {
                            try
                            {
                                sockaddr_storage client_addr;
                                socklen_t client_addr_len = sizeof(client_addr);
                                auto cfd = ::accept4(listener_socket->get_fd(), reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
                                if (cfd < 0)
                                {
                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        break;
                                    throw std::runtime_error("accept4 failed: " + std::string(strerror(errno)));
                                    break;
                                }

                                // Optional: disable Nagle for latency-sensitive workloads.
                                // int one = 1; setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

                                if (add_epoll(cfd, EPOLLIN | EPOLLET) < 0)
                                {
                                    close_socket(cfd);
                                    throw std::runtime_error("epoll_ctl ADD conn error: " + std::string(strerror(errno)));
                                    continue;
                                }
                                auto connptr = std::make_shared<connection>(file_descriptor(cfd), listener_socket->get_bound_address(), socket_address(client_addr));
                                conns.emplace(cfd, epoll_connection{connptr});
                                on_connection_opened(connptr);
                            }
                            catch (const std::exception &e)
                            {
                                on_exception_occurred(e);
                                // ignore errors;
                            }
                        }
                        continue;
                    }

                    auto it = conns.find(fd);
                    if (it == conns.end())
                    {
                        continue;
                    }
                    epoll_connection &c = it->second;
                    // Closing Connection
                    if (ev & (EPOLLERR | EPOLLHUP))
                    {
                        close_conn(fd);
                        continue;
                    }
                    if (ev & HAMZA_CUSTOM_CLOSE_EVENT)
                    {
                        close_conn(fd);
                        continue;
                    }
                    // Reading messages from connections
                    if (ev & EPOLLIN)
                    {
                        try
                        {
                            char buf[64 * 1024];
                            while (true)
                            {
                                ssize_t m = ::recv(fd, buf, sizeof(buf), 0);
                                if (m > 0)
                                {
                                    on_message_received(c.conn, data_buffer(buf, m));
                                }
                                else if (m == 0)
                                {
                                    // peer closed
                                    close_conn(fd);
                                    goto next_event;
                                }
                                else
                                {
                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        break;
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

                    // Flushing writes
                    if (!c.outq.empty())
                    {
                        // Try to flush immediately to avoid extra EPOLLOUT wakeup
                        if (flush_writes(c))
                        {
                            if (c.want_write)
                            {
                                c.want_write = false;
                                mod_epoll(fd, EPOLLIN | EPOLLET);
                            }
                        }
                        else
                        {
                            if (!c.want_write)
                            {
                                c.want_write = true;
                                mod_epoll(fd, EPOLLIN | EPOLLOUT | EPOLLET);
                            }
                        }
                    }
                    if (ev & EPOLLOUT)
                    {
                        if (flush_writes(c))
                        {
                            c.want_write = false;
                            mod_epoll(fd, EPOLLIN | EPOLLET);
                        }
                    }

                next_event:
                    continue;
                }
            }

            on_shutdown_success();
        }

    protected:
        // interface to allow derived classes to close connetion
        void close_connection(std::shared_ptr<connection> conn) override
        {
            auto c = conns.find(conn->get_fd());
            int fd = conn->get_fd();
            if (c == conns.end())
                return;
            mod_epoll(fd, HAMZA_CUSTOM_CLOSE_EVENT);
        }

        // interface to allow derived classes to send messages
        void send_message(std::shared_ptr<connection> conn, const data_buffer &db) override
        {
            int fd = conn->get_fd();
            auto it = conns.find(fd);
            if (it == conns.end())
            {
                return;
            }
            epoll_connection &c = it->second;
            c.outq.emplace_back(db.to_string());

            // single epoll that I have a read
            mod_epoll(fd, EPOLLOUT);
        }

        virtual void on_exception_occurred(const std::exception &e) override
        {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
        virtual void on_connection_opened(std::shared_ptr<connection> conn) override
        {
            std::cout << "Client Connected:\n";
            std::cout << "\t Client " << conn->get_fd() << " connected." << std::endl;
        }
        virtual void on_connection_closed(std::shared_ptr<connection> conn) override
        {
            std::cout << "Client Disconnected:\n";
            std::cout << "\t Client " << conn->get_fd() << " disconnected." << std::endl;
        }
        virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override
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
        virtual void on_listen_success() override
        {
            std::cout << "Listening on " << listener_socket->get_fd() << std::endl;
        }
        virtual void on_shutdown_success() override
        {
            std::cout << "Server Shutdown Successful" << std::endl;
        }

    public:
        epoll_server(int max_fds)
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

        virtual void listen(int timeout = 1000) override
        {
            epoll_loop(timeout);
        }

        virtual std::shared_ptr<socket> make_listener_socket(uint16_t port, const char *ip = "0.0.0.0", int backlog = SOMAXCONN)
        {
            try
            {
                auto sock_ptr = std::make_shared<socket>(Protocol::TCP);

                sock_ptr->set_reuse_address(true);
                sock_ptr->set_non_blocking(true);
                sock_ptr->set_close_on_exec(true);
                sock_ptr->bind(socket_address(hamza_socket::port(port), hamza_socket::ip_address(ip)));
                sock_ptr->listen(backlog);

                // Optional: Enable per-CPU accept queues for multi-process models (one process per core).
                // If you run multiple worker *processes*, uncomment this:
                // sock_ptr->set_option(SOL_SOCKET, SO_REUSEPORT, 1);

                // Optional: reduce wakeups by notifying only when data arrives (Linux-specific).
                // int defer_secs = 1;
                // sock_ptr->set_option(IPPROTO_TCP, TCP_DEFER_ACCEPT, defer_secs);

                return sock_ptr;
            }
            catch (const socket_exception &e)
            {
                std::cerr << "Socket error: " << e.what() << std::endl;
                return nullptr;
            }
        }
        virtual bool register_listener_socket(std::shared_ptr<socket> sock_ptr)
        {
            listener_socket = sock_ptr;
            int lfd = sock_ptr->get_fd();
            if (add_epoll(lfd, EPOLLIN | EPOLLET) != 0)
            {
                return false;
            }
            return true;
        }
        virtual void stop_server() override
        {
            g_stop = 1;
        }

        virtual ~epoll_server()
        {
            for (auto &[fd, _] : conns)
                close_socket(fd);
            if (listener_socket)
                close_socket(listener_socket->get_fd());
            if (epoll_fd != -1)
                close_socket(epoll_fd);
        }
    };

}

#endif
