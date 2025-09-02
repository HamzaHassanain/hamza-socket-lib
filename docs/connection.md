# connection

Source: `includes/connection.hpp`

Synopsis

The `connection` class represents an established TCP connection. It is a move-only RAII wrapper that owns a `file_descriptor` and stores both local and remote `socket_address` values. It exposes small synchronous I/O helpers (`send`, `receive`), explicit close semantics, and raw descriptor access for integration with event loops.

Key characteristics

- Move-only: copy construction and copy assignment are deleted; move operations transfer ownership.
- RAII: destructor calls `close()` to release resources.
- Thin I/O helpers: `send()` and `receive()` are small wrappers over the platform `send`/`recv` syscalls.
- Not thread-safe: callers must synchronize external access.

Constructors & destructor

Signature

- `connection(file_descriptor fd, const socket_address &local_addr, const socket_address &remote_addr)`

Parameters

- `fd` — a `file_descriptor` that the `connection` takes ownership of.
- `local_addr` — local endpoint address (copy stored).
- `remote_addr` — remote endpoint address (copy stored).

Behavior

- Validates the provided `fd`. If `fd.get()` equals `INVALID_SOCKET_VALUE` or `SOCKET_ERROR_VALUE` a `socket_exception` with type `ConnectionCreation` is thrown.

Signature

- `~connection()`

Behavior

- Calls `close()` to ensure the underlying socket is released.

Move semantics

Signature

- `connection(connection &&other) noexcept`
- `connection &operator=(connection &&other) noexcept`

Behavior

- Transfer ownership of the internal `file_descriptor` and address fields. The moved-from object has its `is_open` set to `false` and its descriptor invalidated.

Public API (function-level detail)

#### int get_fd() const

- Signature: `int get_fd() const`
- Description: Return the raw integer socket descriptor owned by the connection.
- Returns: Raw fd value. After `close()` or a move, this may be `INVALID_SOCKET_VALUE`.
- Example:

```cpp
int fd = conn.get_fd();
if (fd >= 0) { /* register fd with epoll/poll/select */ }
```

#### std::size_t send(const data_buffer &data)

- Signature: `std::size_t send(const data_buffer &data)`
- Description: Transmit bytes on the TCP connection using a single `send` syscall.
- Parameters: `data` — buffer of bytes to send.
- Returns: Number of bytes actually sent. Returns `0` if the connection is closed or fd is invalid.
- Exceptions: Throws `socket_exception` with type `SocketWrite` on syscall failure (when `send` returns `SOCKET_ERROR_VALUE`). The exception includes the fd and platform error text.
- Notes:
  - The implementation performs a single `send` call. Partial writes can occur. If you must guarantee full transmission, call `send()` in a loop until all bytes are sent or an exception is thrown.
- Example (simple loop to ensure full write):

```cpp
std::size_t total = 0;
while (total < buf.size()) {
    std::size_t n = conn.send(hh_socket::data_buffer(buf.data() + total, buf.size() - total));
    total += n;
}
```

#### data_buffer receive()

- Signature: `data_buffer receive()`
- Description: Read available bytes from the connection using a single `recv` syscall.
- Returns: A `data_buffer` containing received bytes. Returns an empty `data_buffer` in these cases:
  - The connection is closed or fd invalid.
  - Peer closed the connection (EOF, `recv` returned 0).
  - Non-blocking socket had no data available (`EAGAIN`/`EWOULDBLOCK`) or the call was interrupted (`EINTR`).
- Exceptions: Throws `socket_exception` with type `SocketRead` for read errors other than the non-fatal conditions above. The exception message includes the fd and platform error text.
- Notes:
  - An empty buffer can mean either "no data now" or EOF; callers that need to distinguish must observe the event loop (e.g., detect hang-up events) or rely on protocol state.
- Example (simple receive check):

```cpp
hh_socket::data_buffer msg = conn.receive();
if (msg.empty()) {
    // either no data now or peer closed — check event loop/hup or continue
} else {
    // process msg
}
```

#### void close()

- Signature: `void close()`
- Description: Close the underlying socket if open, invalidate the file descriptor, and mark the connection closed. Safe to call multiple times.
- Notes: Internally uses the cross-platform helper `close_socket(fd.get())` and then invalidates the stored `file_descriptor`.
- Example:

```cpp
conn.close();
// after this conn.is_connection_open() == false
```

#### bool is_connection_open() const

- Signature: `bool is_connection_open() const`
- Description: Return whether the connection is currently open.
- Returns: `true` if open; `false` after `close()` or a move-from.

#### socket_address get_remote_address() const
#### socket_address get_local_address() const

- Signature: `socket_address get_remote_address() const` / `socket_address get_local_address() const`
- Description: Return copies of the stored remote and local addresses.
- Example:

```cpp
auto remote = conn.get_remote_address();
std::cout << "Remote: " << remote.to_string() << '\n';
```

Exceptions & error model

- Uses `hh_socket::socket_exception` for fatal socket errors.
- Typical `type` values thrown by `connection` methods:
  - `ConnectionCreation` — invalid descriptor passed to constructor.
  - `SocketWrite` — `send()` syscall failure.
  - `SocketRead` — `receive()` syscall failure (except non-fatal `EAGAIN`/`EWOULDBLOCK`/`EINTR`).

Implementation notes

- The class keeps I/O helpers minimal; server frameworks (for example `epoll_server`) are responsible for readiness notification and will call `receive()`/`send()` when it is appropriate.
- `receive()` and `send()` currently perform a single system call each. For non-blocking operation they return immediately when no data is available or when partial writes occur.
- Because `get_fd()` exposes the raw descriptor, the `connection` integrates with poll/epoll/kqueue/io_uring-based event loops.

Thread-safety

- Not thread-safe. A common pattern is for a single event-loop thread to own each `connection`.

Examples

1. Synchronous echo (single-shot)

```cpp
// conn is a valid hh_socket::connection
hh_socket::data_buffer buf = conn.receive();
if (!buf.empty()) {
    conn.send(buf); // echo
}
```

2. Non-blocking event-driven handler (conceptual)

```cpp
// fd is readable
hh_socket::data_buffer buf = conn.receive();
if (buf.empty()) {
    // no data or peer closed
    // check event flags: if HUP then treat as closed, otherwise continue
} else {
    // queue buf for sending or process inline
}
```

3. Move semantics example

```cpp
hh_socket::connection c1 = make_connection(...);
hh_socket::connection c2 = std::move(c1);
// c1.is_connection_open() == false
// c2 now owns fd and addresses
```

4. Example: using `connection` with `socket` (accept + echo, non-blocking accept)

```cpp
// Demonstrates creating a listening socket, accepting a connection and
// using the returned hh_socket::connection to receive and send data.

#include <iostream>
#include <chrono>

#include "includes/utilities.hpp"
#include "includes/socket_address.hpp"
#include "includes/port.hpp"
#include "includes/socket.hpp"
#include "includes/connection.hpp"

using namespace hh_socket;

int main() {
    if (!initialize_socket_library()) return 1;

    const uint16_t port_num = 9091;
    socket_address sa(port(port_num));

    // Create, bind and listen
    socket server(sa, Protocol::TCP);
    server.set_reuse_address(true);
    server.set_non_blocking(true); // use non-blocking accept
    server.listen(128);

    std::cout << "Listening on port " << port_num << "\n";

    bool running = true;
    while (running) {
        // Wait for new connections using select on the listening fd (simple example)
        int listen_fd = server.get_fd();
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        timeval tv{1, 0}; // 1 second timeout

        int n = select(listen_fd + 1, &rfds, nullptr, nullptr, &tv);
        if (n > 0 && FD_ISSET(listen_fd, &rfds)) {
            // Try accepting (non-blocking). accept(true) returns nullptr if no pending connection.
            auto conn = server.accept(true);
            if (!conn) continue;

            std::cout << "Accepted from " << conn->get_remote_address().to_string() << "\n";

            // Simple per-connection handling: read once and echo back
            try {
                auto buf = conn->receive();
                if (!buf.empty()) {
                    conn->send(buf);
                }
            } catch (const socket_exception &ex) {
                std::cerr << "Connection error: " << ex.what() << '\n';
            }

            conn->close();
        }

        // Application logic, shutdown check or housekeeping could be here
    }

    cleanup_socket_library();
    return 0;
}
```

Best practices & common pitfalls

- Do not treat an empty `data_buffer` as definitive peer close — it may simply mean "would block". Use event-loop signals (EPOLLHUP/EPOLLERR or equivalent) to disambiguate.
- If your application requires guaranteed delivery of a buffer, implement a loop that calls `send()` until all bytes are transmitted.
- Avoid using a moved-from `connection` object.
