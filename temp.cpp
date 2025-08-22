// g++ -O2 -std=gnu++17 -Wall -Wextra -pedantic epoll_server.cpp -o epoll_server
// ./epoll_server 0.0.0.0 8080
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

namespace
{

    volatile sig_atomic_t g_stop = 0;
    void on_sigint(int) { g_stop = 1; }

    int set_rlimit_nofile(rlim_t soft, rlim_t hard)
    {
        struct rlimit rl{soft, hard};
        return setrlimit(RLIMIT_NOFILE, &rl);
    }

    int make_listen_socket(const char *ip, uint16_t port, int backlog)
    {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0)
        {
            perror("socket");
            return -1;
        }

        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        {
            perror("setsockopt SO_REUSEADDR");
            ::close(fd);
            return -1;
        }

        // Optional: Enable per-CPU accept queues for multi-process models (one process per core).
        // If you run multiple worker *processes*, uncomment this:
        // if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0) {
        //     perror("setsockopt SO_REUSEPORT");
        // }

        // Optional: reduce wakeups by notifying only when data arrives (Linux-specific).
        // int defer_secs = 1;
        // setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &defer_secs, sizeof(defer_secs));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
        {
            std::cerr << "inet_pton failed\n";
            ::close(fd);
            return -1;
        }
        if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            ::close(fd);
            return -1;
        }
        if (::listen(fd, backlog) < 0)
        {
            perror("listen");
            ::close(fd);
            return -1;
        }
        return fd;
    }

    struct Conn
    {
        int fd;
        std::deque<std::string> outq; // queued writes
        bool want_write = false;
    };

    int add_epoll(int ep, int fd, uint32_t ev)
    {
        epoll_event e{};
        e.events = ev;
        e.data.fd = fd;
        return epoll_ctl(ep, EPOLL_CTL_ADD, fd, &e);
    }
    int mod_epoll(int ep, int fd, uint32_t ev)
    {
        epoll_event e{};
        e.events = ev;
        e.data.fd = fd;
        return epoll_ctl(ep, EPOLL_CTL_MOD, fd, &e);
    }
    int del_epoll(int ep, int fd)
    {
        return epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
    }

    void close_conn(int ep, std::unordered_map<int, Conn> &conns, int fd)
    {
        del_epoll(ep, fd);
        ::close(fd);
        conns.erase(fd);
    }

    bool flush_writes(int ep, Conn &c)
    {
        while (!c.outq.empty())
        {
            std::string &front = c.outq.front();
            if (front.empty())
            {
                c.outq.pop_front();
                continue;
            }
            ssize_t n = ::send(c.fd, front.data(), front.size(), 0);
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

} // namespace

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }
    const char *ip = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    // Raise FD limit (make sure shell ulimit is compatible)
    set_rlimit_nofile(1 << 20, 1 << 20); // try up to ~1M, ignore failure here

    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep == -1)
    {
        perror("epoll_create1");
        return 1;
    }

    const int backlog = 32768; // large backlog for bursts
    int lfd = make_listen_socket(ip, port, backlog);
    if (lfd < 0)
        return 1;

    // Level or edge? We choose EPOLLET for fewer wakeups (must read/write until EAGAIN).
    if (add_epoll(ep, lfd, EPOLLIN | EPOLLET) < 0)
    {
        perror("epoll_ctl ADD listen");
        return 1;
    }

    std::unordered_map<int, Conn> conns;
    std::vector<epoll_event> events(4096);

    while (!g_stop)
    {
        int n = epoll_wait(ep, events.data(), (int)events.size(), /*timeout*/ 1000);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
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

            if (fd == lfd)
            {
                // Accept as many as possible (edge-triggered accept loop)
                while (true)
                {
                    int cfd = accept4(lfd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (cfd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        // EMFILE/ENFILE: optionally shed load or use accept-murder strategy
                        perror("accept4");
                        break;
                    }

                    // Optional: disable Nagle for latency-sensitive workloads.
                    // int one = 1; setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

                    if (add_epoll(ep, cfd, EPOLLIN | EPOLLET) < 0)
                    {
                        perror("epoll_ctl ADD conn");
                        ::close(cfd);
                        continue;
                    }
                    conns.emplace(cfd, Conn{cfd});
                }
                continue;
            }

            auto it = conns.find(fd);
            if (it == conns.end())
            {
                // spurious?
                continue;
            }
            Conn &c = it->second;

            if (ev & (EPOLLERR | EPOLLHUP))
            {
                close_conn(ep, conns, fd);
                continue;
            }

            if (ev & EPOLLIN)
            {
                // Read until EAGAIN
                char buf[64 * 1024];
                while (true)
                {
                    ssize_t m = ::recv(fd, buf, sizeof(buf), 0);
                    if (m > 0)
                    {
                        // Example: echo input back
                        c.outq.emplace_back(buf, buf + m);
                    }
                    else if (m == 0)
                    {
                        // peer closed
                        close_conn(ep, conns, fd);
                        goto next_event;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        close_conn(ep, conns, fd);
                        goto next_event;
                    }
                }
            }

            if (!c.outq.empty())
            {
                // Try to flush immediately to avoid extra EPOLLOUT wakeup
                if (flush_writes(ep, c))
                {
                    if (c.want_write)
                    {
                        c.want_write = false;
                        mod_epoll(ep, fd, EPOLLIN | EPOLLET);
                    }
                }
                else
                {
                    if (!c.want_write)
                    {
                        c.want_write = true;
                        mod_epoll(ep, fd, EPOLLIN | EPOLLOUT | EPOLLET);
                    }
                }
            }

            if (ev & EPOLLOUT)
            {
                if (flush_writes(ep, c))
                {
                    c.want_write = false;
                    mod_epoll(ep, fd, EPOLLIN | EPOLLET);
                }
            }

        next_event:
            continue;
        }
    }

    // Cleanup
    for (auto &[fd, _] : conns)
        ::close(fd);
    ::close(lfd);
    ::close(ep);
    return 0;
}
