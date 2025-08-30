# socket (High-level cross-platform socket wrapper)

Source: `includes/socket.hpp` and `src/socket.cpp`

This document describes the `hh_socket::socket` class in detail. It explains each public method, constructor, members, behavior, implementation notes, platform-specific differences, typical error conditions and usage examples. The implementation uses low-level OS sockets (BSD sockets on Unix, Winsock on Windows) and throws `hh_socket::socket_exception` on errors.

Overview and members

- Purpose: a RAII wrapper and convenience layer for TCP/UDP sockets with cross-platform abstractions and safety checks.
- Ownership: move-only type. Copy constructor and copy assignment are deleted to prevent duplicate ownership of OS socket resources.

Private members (summary)

- `socket_address addr` — the bound or target address for the socket.
- `file_descriptor fd` — wrapper for the raw socket handle (platform `socket_t`).
- `Protocol protocol` — enum value (TCP or UDP) describing socket semantics.
- `bool is_open{true}` — boolean flag used by `is_connected()` and `disconnect()`.

Common error/reporting behavior

- Errors in system calls are wrapped and rethrown as `socket_exception` with a descriptive message and a type code (e.g. "SocketCreation", "SocketBinding", "SocketListening").
- Error messages include `get_error_message()` output to capture platform-specific error text.

Platform macros used in implementation

- `socket_errno()` maps to `errno` on Unix, `WSAGetLastError()` on Windows.
- `SOCKET_WOULDBLOCK` / `SOCKET_AGAIN` are mapped to EWOULDBLOCK/EAGAIN on Unix and corresponding WSA codes on Windows.

Constructors

1. explicit socket(const Protocol &protocol)

- Purpose: Create an unbound socket of the given protocol (TCP/UDP).
- Implementation:
  - Calls `::socket(AF_INET, static_cast<int>(protocol), 0)` — currently always creates an IPv4 socket (AF_INET) in this constructor.
  - Checks the returned file descriptor with `is_valid_socket()`.
  - Stores the descriptor in the `file_descriptor` wrapper `fd`.
- Errors:
  - Throws `socket_exception("Invalid File Descriptor", "SocketCreation", __func__)` if socket creation fails.
- Notes:
  - The default parameter in implementation is `Protocol::UDP` (per cpp definition), so calling `socket(someProtocol)` or `socket()` may default to UDP when used directly in source file.
  - If you need an IPv6 socket or a specific domain, use the `socket(const socket_address&, Protocol)` constructor which uses `addr.get_family().get()` to choose the domain.

Example

```cpp
hh_socket::socket s(hh_socket::Protocol::TCP);
```

2. explicit socket(const socket_address &addr, const Protocol &protocol)

- Purpose: Create and immediately bind a socket to the provided `socket_address`.
- Implementation:
  - Calls `::socket(addr.get_family().get(), static_cast<int>(protocol), 0)` so the socket domain (AF_INET/AF_INET6) matches `addr`.
  - Verifies descriptor and stores in `fd`.
  - Sets `this->protocol = protocol` and calls `this->bind(addr)` to perform the bind operation.
- Errors:
  - Throws `socket_exception` with type "SocketCreation" if socket creation fails.
  - Throws `socket_exception` with type "SocketBinding" if bind fails.

Example

```cpp
hh_socket::socket_address sa(hh_socket::port(8080));
hh_socket::socket srv(sa, hh_socket::Protocol::TCP);
```

Move operations

- Move constructor: Transfers `addr`, `fd`, and `protocol` from `other`. After move the source socket no longer owns the descriptor.
- Move assignment: Transfers ownership and data, protecting against self-assignment.
- Copy operations are deleted to prevent resource duplication.

Key methods (detailed)

#### `bind(const socket_address &addr)`

- Purpose: Bind the socket to a local address (IP + port).
- Implementation:
  - Assigns `this->addr = addr` then calls `::bind(fd.get(), addr.get_sock_addr(), addr.get_sock_addr_len())`.
  - On failure it throws `socket_exception("Failed to bind to address: " + get_error_message(), "SocketBinding", __func__)`.
- Notes:
  - Common failures: EADDRINUSE (address in use), EACCES (permission denied when binding to low ports), EINVAL.
  - Must be called before `listen()` for servers; for UDP you must bind to receive packets on a port.

#### `set_reuse_address(bool reuse)`

- Purpose: Toggle the `SO_REUSEADDR` socket option.
- Implementation:
  - Uses `setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))` with `optval` cast to `const char*` for portability.
  - Throws `socket_exception` with type "SocketOption" if setsockopt fails.
- Notes:
  - On Unix `SO_REUSEADDR` allows immediate reuse of ports in TIME_WAIT; on Windows semantics differ (consider `SO_EXCLUSIVEADDRUSE`). Always call before `bind()`.

#### `set_non_blocking(bool enable)`

- Purpose: Switch the socket between blocking and non-blocking modes.
- Implementation:
  - Windows: calls `ioctlsocket(fd.get(), FIONBIO, &mode)`.
  - Unix: uses `fcntl(fd.get(), F_GETFL)` to get flags, modify `O_NONBLOCK`, and `F_SETFL` to set flags.
  - Throws `socket_exception("Failed to set socket non-blocking mode: " + get_error_message(), "SocketOption", __func__)` on error.
- Notes:
  - Non-blocking sockets cause I/O calls to return immediately with EWOULDBLOCK/EAGAIN when no data is available.
  - Use to integrate with event loops (epoll/select) or to implement timeouts in application code.

#### `set_close_on_exec(bool enable)`

- Purpose: Set or clear the close-on-exec flag so the descriptor is/ is not inherited by child processes.
- Implementation:
  - Windows: uses `GetHandleInformation` and `SetHandleInformation` to clear or set `HANDLE_FLAG_INHERIT`.
  - Unix: uses `fcntl(fd.get(), F_GETFD)` and then `F_SETFD` to set or clear `FD_CLOEXEC`.
  - Wraps system calls in try/catch and converts failures into `socket_exception("Error setting close-on-exec flag: " + e.what(), "SocketSetCloseOnExec", __func__)`.
- Notes:
  - Useful for servers that spawn child processes and want to avoid leaking descriptors.

#### `connect(const socket_address &server_address)`

- Purpose: For TCP, establish a connection to a remote server. For UDP, set the default peer for `send()`/`recv()`.
- Implementation:
  - Calls `::connect(fd.get(), server_address.get_sock_addr(), server_address.get_sock_addr_len())`.
  - On failure throws `socket_exception("Failed to connect to address: " + get_error_message(), "SocketConnection", __func__)`.
- Notes:
  - For non-blocking sockets, `connect()` may return immediately with `EINPROGRESS` and complete later; check SO_ERROR via `getsockopt` to determine final outcome.

#### `listen(int backlog = SOMAXCONN)`

- Purpose: Transition a TCP socket into listening mode to accept incoming connections.
- Implementation:
  - Verifies `protocol == Protocol::TCP`, otherwise throws `socket_exception("Listen is only supported for TCP sockets", "ProtocolMismatch", __func__)`.
  - Calls `::listen(fd.get(), backlog)` and throws `socket_exception("Failed to listen on socket: " + get_error_message(), "SocketListening", __func__)` on failure.
- Notes:
  - `backlog` controls the size of the pending connection queue; `SOMAXCONN` is used by default.

#### `accept(bool NON_BLOCKING = false)`

- Purpose: Accept an incoming TCP connection and return a `std::shared_ptr<connection>` encapsulating the accepted socket.
- Implementation highlights:
  - Validates `protocol == Protocol::TCP`.
  - Prepares `sockaddr_storage client_addr` and calls `::accept()` for blocking or platform-specific non-blocking accept variations:
    - On Unix, `accept4()` is used for non-blocking + cloexec flags (`SOCK_NONBLOCK | SOCK_CLOEXEC`).
    - On Windows, accepted socket is set non-blocking using `ioctlsocket` when requested.
  - After `accept()` it checks for EAGAIN/EWOULDBLOCK and returns `nullptr` for no pending connection in non-blocking mode.
  - Validates the returned `client_fd` and constructs `connection` via `std::make_shared<connection>(file_descriptor(client_fd), this->get_bound_address(), socket_address(client_addr))`.
- Errors:
  - Throws `SocketAcceptance` on failure to accept.
- Notes:
  - The returned `connection` object wraps the accepted descriptor and client address information. The server should manage the lifetime of the returned `connection`.

#### `receive(socket_address &client_addr)  (UDP only)`

- Purpose: Receive a datagram from any sender on a UDP socket and return the data with sender address.
- Implementation:
  - Ensures `protocol == Protocol::UDP` (throws `ProtocolMismatch` otherwise).
  - Calls `::recvfrom(fd.get(), buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&sender_addr), &sender_addr_len)` using a `MAX_BUFFER_SIZE` buffer (64 KiB) to accommodate large UDP datagrams.
  - On error throws `SocketReceive`.
  - On success, wraps the received bytes into `data_buffer` and fills `client_addr = socket_address(sender_addr)`.
- Notes:
  - UDP is connectionless — every `recvfrom` provides the source address which must be used when sending a reply.

#### `send_to(const socket_address &addr, const data_buffer &data)  (UDP only)`

- Purpose: Send a datagram to the specified destination address.
- Implementation:
  - Ensures `protocol == Protocol::UDP`.
  - Calls `::sendto(fd.get(), data.data(), data.size(), 0, addr.get_sock_addr(), addr.get_sock_addr_len())`.
  - Throws `SocketSend` on send error.
  - Checks for partial send and throws `PartialSend` if bytes sent != data.size().
- Notes:
  - UDP typically sends entire datagram in one syscall; partial sends are abnormal and handled as errors here.

#### `get_bound_address() const`

- Purpose: Return the stored `socket_address` this socket is bound to (or constructed with).
- Implementation: returns `addr` by value.

#### `get_fd() const`

- Purpose: Return the raw file descriptor integer for advanced or platform-specific operations.
- Implementation: returns `fd.get()`.
- Notes: Using raw descriptor bypasses safety of `file_descriptor`. Be careful when mixing raw syscalls with wrapper methods.

#### `disconnect()`

- Purpose: Close the socket and mark it as closed. This method is idempotent and safe to call multiple times.
- Implementation:
  - If `is_open` is true: calls `close_socket(fd.get())`, `fd.invalidate()` and sets `is_open = false`.
  - Does not throw — errors are reported via `socket_exception` in internal calls, but `disconnect()` is designed to be noexcept-like (implementation currently doesn't catch exceptions from `close_socket`).
- Notes:
  - After `disconnect()`, this socket object can no longer be used to perform I/O. The destructor calls `disconnect()` to ensure cleanup.

#### `is_connected() const`

- Purpose: Report whether the socket is currently marked as open by this wrapper.
- Implementation: returns the `is_open` flag.
- Caveat:
  - This is a lightweight wrapper flag and does not guarantee that the TCP connection is still established with the peer. Use diagnostic helpers (e.g., `is_socket_connected()` in `utilities`) or attempt a zero-byte send/recv to verify the connection.

#### `set_option(int level, int optname, int optval)`

- Purpose: Generic wrapper around `setsockopt()` for setting integer-based socket options.
- Implementation: performs `setsockopt(fd.get(), level, optname, reinterpret_cast<const char*>(&optval), sizeof(optval))` and throws `socket_exception` on failure.
- Notes: Works for common integer options like `TCP_NODELAY` (with `IPPROTO_TCP`) or `SO_KEEPALIVE` (with `SOL_SOCKET`).

#### `operator<`

- Purpose: Provide ordering based on file descriptor value using `fd` comparison (delegates to `file_descriptor` operator<).
- Usage: Allows use of `socket` objects in ordered containers.

#### `~socket()` (Destructor)

- Purpose: Ensure resource cleanup on object destruction.
- Implementation: Calls `disconnect()` so the OS descriptor is closed and `is_open` is cleared.

Examples and recommended usage

Basic TCP server bind/listen/accept:

```cpp
hh_socket::socket_address sa(hh_socket::port(8080));
hh_socket::socket srv(sa, hh_socket::Protocol::TCP);
srv.set_reuse_address(true);
srv.listen(128);
while (true) {
    auto conn = srv.accept();
    if (conn) {
        // handle connection
    }
}
```

Simple UDP receive/send:

```cpp
hh_socket::socket udp(hh_socket::Protocol::UDP);
udp.set_reuse_address(true);
udp.bind(hh_socket::socket_address(hh_socket::port(9000)));
hh_socket::socket_address sender;
auto buf = udp.receive(sender);
udp.send_to(sender, buf);
```

## Quick checklist for robust server creation

Follow these steps when creating a production-ready server:

1. Initialize socket library on Windows:
   - Call `hh_socket::initialize_socket_library()` before any socket operations.
2. Create a `socket_address` with the desired family, IP and port (prefer explicit `family` if IPv6 required).
3. Construct a `hh_socket::socket` with the `socket_address` and `hh_socket::Protocol::TCP`.
4. Set `SO_REUSEADDR` (and consider `SO_EXCLUSIVEADDRUSE` on Windows) before `bind()` to avoid address-in-use on restart:
   - `srv.set_reuse_address(true);`
5. Bind the socket (done automatically by the `socket(addr, protocol)` constructor in this library) and call `set_non_blocking(true)` if integrating with an event loop.
6. Call `listen(backlog)` to start accepting connections.
7. Use an accept loop that handles `EWOULDBLOCK`/`EAGAIN` for non-blocking mode, or block in `accept()` in a dedicated thread.
8. For each accepted `connection`:
   - Use `connection->receive()` and `connection->send()` (or your application logic) to process data.
   - Close `connection` when finished.
9. On shutdown: call `disconnect()` on listening sockets and `cleanup_socket_library()` on Windows.

## Minimal echo server example (complete)

This example shows a simple single-threaded TCP echo server that accepts connections and echoes received data back to clients using the library's `socket` and `connection` APIs. It is intentionally minimal to demonstrate the common flow; production code should add error handling, timeouts, and concurrency.

Save as `examples/echo_server.cpp` and compile against your project.

```cpp
#include <iostream>
#include <memory>

#include "includes/utilities.hpp"
#include "includes/socket_address.hpp"
#include "includes/port.hpp"
#include "includes/ip_address.hpp"
#include "includes/family.hpp"
#include "includes/socket.hpp"
#include "includes/connection.hpp"

using namespace hh_socket;

int main() {
    // Initialize socket subsystem on Windows (no-op on Unix)
    if (!initialize_socket_library()) {
        std::cerr << "Failed to initialize socket library" << std::endl;
        return 1;
    }

    const uint16_t listen_port = 9090;

    try {
        // Create address (IPv4 any) and server socket bound to that address
        socket_address sa(port(listen_port));
        socket server(sa, Protocol::TCP);

        // Make server reusable and start listening
        server.set_reuse_address(true);
        server.listen(128);

        std::cout << "Echo server listening on port " << listen_port << std::endl;

        while (true) {
            // Accept a new connection (blocking)
            auto conn = server.accept();
            if (!conn) continue; // no connection accepted

            std::cout << "Accepted connection from " << conn->get_remote_address().to_string() << std::endl;

            // Simple echo loop for this connection
            try {
                while (conn->is_connection_open()) {
                    data_buffer buf = conn->receive();
                    if (buf.size() == 0) break; // peer closed

                    // Echo back
                    conn->send(buf);
                }
            } catch (const socket_exception &ex) {
                std::cerr << "Connection error: " << ex.what() << " (" << ex.type() << ")" << std::endl;
            }

            // Ensure connection is closed
            conn->close();
            std::cout << "Connection closed" << std::endl;
        }

    } catch (const socket_exception &ex) {
        std::cerr << "Server error: " << ex.what() << " (" << ex.type() << ")" << std::endl;
        cleanup_socket_library();
        return 1;
    }

    // Clean up Winsock on Windows
    cleanup_socket_library();
    return 0;
}
```

Build instructions (Unix-like)

```bash
g++ -std=c++17 -I. examples/echo_server.cpp -o echo_server
```

On Windows use your normal MSVC project settings and ensure Winsock libs are linked.

Notes

- This example uses blocking `accept()` and a simple per-connection loop. For higher throughput, process each connection on a worker thread or integrate the socket with an event loop (epoll/kqueue/IOCP).
