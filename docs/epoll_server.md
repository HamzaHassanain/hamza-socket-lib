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
- `conns` - Map of file descriptors to their `epoll_connection` state
- `events` - Vector for batch event processing from epoll_wait
- `g_stop` - Atomic stop flag for graceful shutdown

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

#### `epoll_loop(int timeout)`

The heart of the server - main event processing loop:

1. **Wait for events**: `epoll_wait(epoll_fd, events.data(), events.size(), timeout)`
2. **Handle timeout**: Call `on_waiting_for_activity()` if no events
3. **Auto-resize event buffer**: Double size if all event slots were used
4. **Process each event**:
   - **New connections**: Call `try_accept()` for listener socket
   - **Incoming data**: Call `try_read()` for EPOLLIN events
   - **Outgoing data**: Call `flush_writes()` for EPOLLOUT events
   - **Connection closure**: Handle EPOLLHUP, EPOLLERR, or custom close events
5. **Repeat**: Continue until `g_stop` is set

#### `try_accept()`

Accepts new connections in a loop (edge-triggered handling):

- Uses `accept4()` with SOCK_NONBLOCK | SOCK_CLOEXEC on Linux
- Creates `connection` object for each accepted client
- Adds connection to epoll monitoring with EPOLLIN | EPOLLET
- Calls `on_connection_opened()` callback
- Handles EAGAIN/EWOULDBLOCK when no more connections are pending

#### `try_read(epoll_connection &c)`

Reads incoming data from a connection:

- Calls `connection->receive()` to read available data
- Handles partial reads due to non-blocking sockets
- Calls `on_message_received()` with received data
- Handles connection errors and closure
- Catches exceptions to prevent server crashes

#### `flush_writes(epoll_connection &c)`

Sends queued outbound messages:

- Processes each message in the output queue in order
- Uses `::send()` with MSG_NOSIGNAL to avoid SIGPIPE
- Handles partial sends by updating message content
- Stops on EAGAIN/EWOULDBLOCK (socket buffer full)
- Disables EPOLLOUT monitoring when queue is empty

## Virtual callback implementations

#### `on_exception_occurred(const std::exception &e) override`

Default implementation logs exceptions to stderr. Override for custom error handling:

```cpp
void on_exception_occurred(const std::exception &e) override {
    syslog(LOG_ERR, "Server exception: %s", e.what());
    // Implement recovery logic if needed
}
```

#### `on_connection_opened(std::shared_ptr<connection> conn) override`

Default implementation logs connection information. Override for custom logic:

```cpp
void on_connection_opened(std::shared_ptr<connection> conn) override {
    active_connections++;
    rate_limiter.add_connection(conn->get_remote_address());
    send_message(conn, data_buffer("Welcome to our service!\n"));
}
```

#### `on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override`

Default implementation spawns detached threads for message processing. **Production servers should override this** with a thread pool or synchronous processing:

```cpp
void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
    // Better: use a thread pool instead of spawning threads
    thread_pool.submit([=] {
        auto response = process_request(db);
        send_message(conn, response);
    });
}
```

## Helper methods

#### `add_epoll(int fd, uint32_t ev)` / `mod_epoll(int fd, uint32_t ev)` / `del_epoll(int fd)`

Low-level epoll control wrappers:

- `add_epoll`: Register new file descriptor for monitoring
- `mod_epoll`: Change events being monitored for existing descriptor
- `del_epoll`: Remove file descriptor from monitoring

#### `close_conn(int fd)`

Complete connection cleanup:

- Removes from epoll monitoring
- Calls `on_connection_closed()` callback
- Closes the socket using `close_socket()`
- Removes from connection map

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
