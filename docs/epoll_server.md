# epoll_server (High-performance epoll-based TCP server)

Source: `includes/epoll_server.hpp` and `src/epoll_server.cpp`

`epoll_server` is a concrete implementation of the `tcp_server` interface that provides a high-performance, scalable TCP server using Linux's epoll mechanism (with wepoll fallback on Windows). It's designed to handle thousands of concurrent connections efficiently using edge-triggered epoll and non-blocking I/O throughout.

## Architecture overview

- **Single-threaded event loop**: Uses epoll_wait() to efficiently monitor all file descriptors
- **Edge-triggered epoll**: Minimizes system calls and maximizes performance
- **Non-blocking I/O**: All socket operations are non-blocking to prevent thread blocking
- **Connection state tracking**: Each connection has associated state (queues, flags) in `epoll_connection`
- **Asynchronous message sending**: Outbound messages are queued and sent when sockets are writable

## Platform support

- **Linux**: Native epoll support for optimal performance
- **Windows**: Uses wepoll library for epoll-like functionality (reduced performance)
- **Cross-platform**: Conditional compilation handles platform differences

## Key data structures

### `epoll_connection`

Internal state for each active connection:

- `std::shared_ptr<connection> conn` - The connection object
- `std::deque<std::string> outq` - Queue of pending outbound messages
- `bool want_write` - Flag indicating EPOLLOUT monitoring is enabled
- `bool want_close` - Flag indicating connection should be closed

### Private members

- `epoll_fd` - The epoll file descriptor (Linux) or HANDLE (Windows)
- `listener_socket` - Shared pointer to the listening socket
- `events` - Vector for batch event processing from epoll_wait
- `g_stop` - Atomic stop flag for graceful shutdown

### Protected members

- `conns` - Map of file descriptors to their `epoll_connection` state

## Constructor and lifecycle

#### `epoll_server(int max_fds)`

- **Purpose**: Initialize the epoll server with specified file descriptor limit.
- **Implementation**:
  - Sets process RLIMIT_NOFILE (Linux only) to allow more concurrent connections
  - Creates epoll instance with EPOLL_CLOEXEC flag
  - Allocates initial event buffer (4096 events)
  - Validates epoll creation and throws on failure

Example:

```cpp
epoll_server server(8192); // Support up to 8192 file descriptors
```

#### `~epoll_server()`

- **Purpose**: Clean up all resources during destruction.
- **Implementation**:
  - Closes all active client connections
  - Closes listener socket if present
  - Closes epoll file descriptor

## Public interface

#### `listen(int timeout) override`

- **Purpose**: Start the main event loop and begin accepting connections.
- **Implementation**: Calls `epoll_loop(timeout)` which blocks until `stop_server()` is called.
- **Threading**: Blocks the calling thread - typically run in main thread or dedicated server thread.

#### `register_listener_socket(std::shared_ptr<socket> sock_ptr)`

- **Purpose**: Register a pre-configured listening socket with the epoll server.
- **Prerequisites**:
  - Socket must be bound to an address
  - Socket must be in listening state (`listen()` called)
  - Socket should be configured with desired options
- **Implementation**:
  - Stores socket reference internally
  - Adds socket to epoll monitoring with EPOLLIN | EPOLLET
  - Uses edge-triggered mode for maximum performance

Example:

```cpp
auto listener = make_listener_socket(8080, "0.0.0.0");
if (!server.register_listener_socket(listener)) {
    throw std::runtime_error("Failed to register listener");
}
```

#### `stop_server() override`

- **Purpose**: Signal graceful shutdown of the server.
- **Implementation**: Sets `g_stop = 1` which causes the event loop to exit after processing current events.
- **Threading**: Thread-safe - can be called from signal handlers or other threads.

## Protected interface (tcp_server implementation)

#### `close_connection(std::shared_ptr<connection> conn) override`

- **Purpose**: Request closure of a specific connection.
- **Implementation**:
  - Sets `want_close = true` for the connection
  - Uses custom epoll event (HAMZA_CUSTOM_CLOSE_EVENT) to signal the main loop
  - Actual closure happens in the next event loop iteration
- **Thread safety**: Safe to call from application threads via epoll signaling mechanism.

#### `send_message(std::shared_ptr<connection> conn, const data_buffer &db) override`

- **Purpose**: Queue a message for asynchronous sending.
- **Implementation**:
  - Adds message to connection's output queue (`outq`)
  - Enables EPOLLOUT monitoring to trigger sending when socket is writable
  - Messages are sent asynchronously in the main event loop
- **Flow control**: Automatic via epoll - sending stops when socket buffers are full.

Example usage:

```cpp
void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
    // Echo the message back
    send_message(conn, db);
}
```

## Core event loop implementation

### void epoll_loop(int timeout = 1000)

- Signature: `void epoll_loop(int timeout = 1000)`
- Description: Main event loop that waits for epoll events and dispatches handlers.
- Behavior (summary):
  1. Call `epoll_wait(epoll_fd, events.data(), events.size(), timeout)`.
  2. If no events, call `on_waiting_for_activity()` and continue.
  3. For each event:
     - If event on listener socket: call `try_accept()`.
     - If event on client socket with EPOLLIN: call `try_read()`.
     - If event on client socket with EPOLLOUT: call `flush_writes()`.
     - If event indicates EPOLLHUP/EPOLLERR or custom close event: call `close_conn(fd)`.
  4. Auto-resize `events` if the returned event count equals capacity.
  5. Loop until `g_stop` is set by `stop_server()`.
- Notes:
  - Designed to be efficient: batch processing, edge-triggered semantics, and dynamic event buffer growth.
  - All I/O should be non-blocking; the loop must drain read/write opportunities fully per event.

### int set_rlimit_nofile(rlim_t soft, rlim_t hard)

- Signature: `int set_rlimit_nofile(rlim_t soft, rlim_t hard)`
- Scope: Linux-only helper (wraps setrlimit)
- Description: Attempt to set the process file descriptor limits (`RLIMIT_NOFILE`) to the requested soft and hard values.
- Returns: `0` on success; `-1` on failure.
- Notes:
  - Called during server initialization to raise the limit when `max_fds` is large.
  - Failing to raise the limit is non-fatal but will reduce the maximum number of concurrent connections.
  - Only available on Linux platforms; no-op on other OSes.

### int add_epoll(int fd, uint32_t ev)

- Signature: `int add_epoll(int fd, uint32_t ev)`
- Description: Register `fd` with the epoll instance for the specified event mask `ev` (for example `EPOLLIN | EPOLLET`).
- Returns: `0` on success; `-1` on failure.
- Behavior:
  - Constructs an `epoll_event` with `ev` and the file descriptor as data.
  - Calls `epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)` and logs/throws on error at the callsite.
  - Uses `EPOLLET` (edge-triggered) in combination with non-blocking sockets.
- Notes:
  - Ensure `fd` is non-blocking before registering in edge-triggered mode.

### int mod_epoll(int fd, uint32_t ev)

- Signature: `int mod_epoll(int fd, uint32_t ev)`
- Description: Change the event mask for an already registered descriptor.
- Returns: `0` on success; `-1` on failure.
- Behavior:
  - Prepares `epoll_event` with the new mask and calls `epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event)`.
  - Used to enable/disable `EPOLLOUT` when the connection's output queue transitions between empty and non-empty.
- Notes:
  - Failing to mod epoll may result in write-ready notifications being missed or excessive notifications.

### int del_epoll(int fd)

- Signature: `int del_epoll(int fd)`
- Description: Remove `fd` from epoll monitoring and ignore errors from `epoll_ctl` if fd is already closed.
- Returns: `0` on success; `-1` on failure (non-fatal if fd already removed).
- Notes:
  - Always called during connection cleanup to avoid stale registrations.

### void close_conn(int fd)

- Signature: `void close_conn(int fd)`
- Description: Perform full cleanup for a connection identified by `fd`.
- Behavior:
  - Remove fd from epoll via `del_epoll(fd)`.
  - If an `epoll_connection` exists in `conns`, call the `on_connection_closed()` callback with the stored `conn` shared pointer.
  - Close the underlying socket using `close_socket()` and invalidate/remove it from `conns`.
- Notes:
  - This function centralizes cleanup logic to ensure callbacks and resource release are consistent.

### void stop_reading_from_connection(std::shared_ptr<connection> conn)

- Signature: `void stop_reading_from_connection(std::shared_ptr<connection> conn)`
- Description: Request that the server stop reading from the given connection.
- Behavior:
  - Sets an internal flag on the connection state indicating reads should be ignored.
  - Typically implemented by disabling `EPOLLIN` via `mod_epoll(fd, flags_without_read)` so the main loop no longer attempts `try_read()` for that connection.
- Notes:
  - Useful for back-pressure, long-running request handling, or when transferring ownership of a connection to another component.

### void close_connection(int fd)

- Signature: `void close_connection(int fd)`
- Description: Overload that closes a connection by file descriptor. Internally resolves the `epoll_connection` and delegates to `close_conn(fd)` logic.
- Notes:
  - Public `close_connection(std::shared_ptr<connection>)` calls into this helper by retrieving the fd from the connection object.

### void try_accept()

- Signature: `void try_accept()`
- Description: Accept as many pending connections as possible from the listening socket (loop until `accept` returns EAGAIN/EWOULDBLOCK).
- Behavior:
  - Uses `accept4()` with `SOCK_NONBLOCK | SOCK_CLOEXEC` on Linux when available; falls back to `accept()` then sets flags.
  - For each accepted client fd:
    - Wrap in a `file_descriptor` and create a `connection` object.
    - Insert into `conns` and register the client fd with epoll (`EPOLLIN | EPOLLET`).
    - Call `on_connection_opened()` callback.
  - On non-blocking `accept` when no more pending connections are available, returns normally.
- Error handling:
  - Handles transient errors (`EAGAIN`, `EWOULDBLOCK`) as normal stop conditions for the accept loop.
  - On other errors logs or reports via `on_exception_occurred()`.

### void try_read(epoll_connection &c)

- Signature: `void try_read(epoll_connection &c)`
- Description: Read all available data from `c.conn` using its `receive()` helper and dispatch messages to `on_message_received()`.
- Behavior:
  - Loop calling `c.conn->receive()` until it returns empty or an error indicates no more data (EAGAIN/EWOULDBLOCK).
  - Aggregate or process each non-empty `data_buffer` returned by `receive()` and call `on_message_received(c.conn, db)` for each chunk.
  - If `receive()` indicates EOF (empty return with underlying closure), mark connection for close and schedule cleanup.
  - Catch `socket_exception` from `receive()` and forward to `on_exception_occurred()` while ensuring the connection is cleaned up.
- Notes:
  - Expect multiple messages per call because edge-triggered epoll requires draining the socket until EAGAIN.

### bool flush_writes(epoll_connection &c)

- Signature: `bool flush_writes(epoll_connection &c)`
- Description: Attempt to send queued outbound messages for connection `c`.
- Returns: `true` if the output queue became empty (all data sent); `false` if the socket would block before sending all data.
- Behavior:
  - While `!c.outq.empty()`:
    - Take the front message (string) and call `::send()` on the socket fd using `MSG_NOSIGNAL` (or platform equivalent) to avoid SIGPIPE.
    - On partial send, update the queued message by removing the sent prefix and return `false` (socket is full).
    - On `EAGAIN`/`EWOULDBLOCK` return `false` (socket buffers are full).
    - On other errors, mark connection for close and propagate error to `on_exception_occurred()`.
  - If the queue empties, clear `want_write` and adjust epoll monitoring to stop watching `EPOLLOUT`.
- Notes:
  - Ensures ordering of messages and preserves any partial message state between iterations.

## Virtual callback implementations

#### `on_exception_occurred(const std::exception &e) override`

Default implementation logs exceptions to stderr. Override for custom error handling:

```cpp
void epoll_server::on_exception_occurred(const std::exception &e)
{
    std::cerr << "Exception occurred: " << e.what() << std::endl;
}
```

#### `on_connection_opened(std::shared_ptr<connection> conn) override`

Default implementation logs connection information. Override for custom logic:

```cpp
void epoll_server::on_connection_opened(std::shared_ptr<connection> conn)
{
    std::cout << "Connection opened: " << conn->get_fd() << std::endl;
}
```

#### `on_connection_closed(std::shared_ptr<connection> conn) override`

Default implementation logs connection closure. Override for custom logic:

```cpp
void epoll_server::on_connection_closed(std::shared_ptr<connection> conn)
{
    std::cout << "Connection closed: " << conn->get_fd() << std::endl;
}
```

#### `on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override`

**Production servers should override this** with a thread pool or synchronous processing:

```cpp
void epoll_server::on_message_received(std::shared_ptr<connection> conn, const data_buffer &db)
{
    std::cout
        << "Message Received from " << conn->get_fd() << ": " << db.to_string() << std::endl;
    std::string message = "Echo " + db.to_string();

    if (db.to_string() == "close\n")
        close_connection(conn);
    else
        send_message(conn, data_buffer(message));
}
```

#### `on_listen_success() override`

Default implementation logs listening socket information. Override for custom logic.

```cpp
void epoll_server::on_listen_success()
{
    std::cout << "Server is listening on port " << listen_port << std::endl;
}
```

#### `on_shutdown_success() override`

Default implementation logs server shutdown information. Override for custom logic.

```cpp
void epoll_server::on_shutdown_success()
{
    std::cout << "Server has shut down successfully." << std::endl;
}
```

## Performance characteristics

- **Scalability**: O(1) event notification via epoll, scales to thousands of connections
- **Memory efficiency**: Minimal per-connection overhead (just the epoll_connection struct)
- **CPU efficiency**: Edge-triggered epoll minimizes system calls
- **Throughput**: Non-blocking I/O prevents thread blocking on slow clients

## Usage example

```cpp
class ChatServer : public epoll_server {
    std::unordered_map<int, std::string> usernames;

protected:
    void on_connection_opened(std::shared_ptr<connection> conn) override {
        send_message(conn, data_buffer("Enter username: "));
    }

    void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
        std::string msg = db.to_string();

        if (usernames.find(conn->get_fd()) == usernames.end()) {
            // First message is username
            usernames[conn->get_fd()] = msg;
            broadcast(usernames[conn->get_fd()] + " joined the chat");
        } else {
            // Regular chat message
            broadcast(usernames[conn->get_fd()] + ": " + msg);
        }
    }

    void on_connection_closed(std::shared_ptr<connection> conn) override {
        auto it = usernames.find(conn->get_fd());
        if (it != usernames.end()) {
            broadcast(it->second + " left the chat");
            usernames.erase(it);
        }
    }

private:
    void broadcast(const std::string &message) {
        data_buffer msg(message);
        for (const auto &[fd, conn_state] : conns) {
            send_message(conn_state.conn, msg);
        }
    }
};

int main() {
    ChatServer server(1024);
    auto listener = make_listener_socket(8080);
    server.register_listener_socket(listener);

    // Handle SIGINT for graceful shutdown
    signal(SIGINT, [](int) { /* call server.stop_server() */ });

    server.listen(1000); // Start server (blocks)
    return 0;
}
```

## Production considerations

- **Thread safety**: The server is single-threaded by design. Use message queues or similar mechanisms for inter-thread communication.
- **Resource limits**: Set appropriate RLIMIT_NOFILE values for high connection counts.
- **Error handling**: Override `on_exception_occurred()` for production-quality error handling and recovery.
- **Message processing**: Replace the default thread-spawning `on_message_received()` with a thread pool or synchronous processing.
- **Monitoring**: Override lifecycle callbacks to integrate with monitoring and metrics systems.
- **Security**: Implement rate limiting, connection limits, and input validation in your derived class.

## Limitations and caveats

- **Linux-specific optimizations**: Some features (like `accept4()`) are Linux-specific; Windows fallback may have reduced performance.
- **Single-threaded**: All I/O happens in one thread - CPU-intensive message processing should be offloaded to worker threads.
- **Memory growth**: The event buffer auto-resizes but never shrinks - consider periodic resets for long-running servers.
- **IPv4 focus**: Current implementation primarily targets IPv4; IPv6 support may require additional configuration.

---

`epoll_server` provides a robust, high-performance foundation for building scalable TCP servers while maintaining clean separation between transport logic and application logic.

## Important Notes

- **For Sending & Receiving Data**: NEVER EVER Use conn->send/receive/close directly. Always use the send_message()/close_connection() method from the epoll_server class. This ensures that data is sent asynchronously and respects flow control.

- **For Connection Management**: Always use the close_connection() method from the epoll_server class to close connections. This ensures proper cleanup and avoids resource leaks.

- **Multi-threading**: The epoll_server class is not thread-safe. If you need to use multiple threads to run event loops, you must ensure that you extend the class and implement your own synchronization mechanisms.

- **For the current implementation**: All I/O operations are performed in a single thread. If you need to offload work to other threads, consider using a thread pool or similar mechanism.
