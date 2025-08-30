# tcp_server (Abstract TCP server interface)

Source: `includes/tcp_server.hpp`

The `tcp_server` class defines an abstract interface for TCP server implementations. It provides a minimal, well-documented contract that concrete server classes (like `epoll_server`) must implement. This separation allows different I/O mechanisms (epoll, select, IOCP, etc.) to share common application-level callback semantics.

## Design philosophy

- **Transport abstraction**: Separate low-level I/O details from application logic.
- **Callback-driven**: Event-based architecture where the server calls application hooks.
- **Minimal interface**: Only essential methods to avoid constraining implementations.
- **Thread-aware**: Documents threading expectations but allows implementations flexibility.

## Class structure

`tcp_server` is a pure abstract base class with:

- **Protected virtual methods**: Callbacks that derived classes must implement and application code overrides.
- **Public virtual methods**: Control interface (`listen()`, `stop_server()`) that implementations provide.
- **No data members**: Interface only, no state.

## Protected virtual methods (callbacks)

These methods define the application interface. Concrete server implementations call these when events occur.

#### `close_connection(std::shared_ptr<connection> conn) = 0`

- **Purpose**: Request closure of a specific client connection.
- **Usage**: Called by application logic when it wants to terminate a connection (e.g., authentication failure, protocol violation).
- **Implementation responsibility**: The concrete server must ensure closure happens safely within its I/O loop model.
- **Threading**: Must be safe to call from application threads; implementations typically queue the closure for the I/O thread.

Example:

```cpp
class MyServer : public epoll_server {
    void handle_auth_failure(std::shared_ptr<connection> conn) {
        close_connection(conn); // Request closure via server
    }
};
```

#### `send_message(std::shared_ptr<connection> conn, const data_buffer &db) = 0`

- **Purpose**: Queue or send data to a specific connection.
- **Usage**: Application calls this to send responses, notifications, or any data to clients.
- **Implementation responsibility**: Handle flow control, queuing, and non-blocking sends. May return immediately and send asynchronously.
- **Threading**: Should be safe to call from application threads.

Example:

```cpp
void handle_request(std::shared_ptr<connection> conn, const data_buffer &request) {
    data_buffer response = process_request(request);
    send_message(conn, response); // Queue for sending
}
```

#### `on_exception_occurred(const std::exception &e) = 0`

- **Purpose**: Notify application of server-level exceptions.
- **Usage**: Override to implement custom error handling, logging, or recovery.
- **Called when**: I/O errors, system call failures, or other server-level exceptions occur.
- **Threading**: Called from the I/O thread context.

Example:

```cpp
void on_exception_occurred(const std::exception &e) override {
    logger.error("Server exception: " + std::string(e.what()));
    if (should_restart_on_error(e)) {
        restart_server();
    }
}
```

#### `on_connection_opened(std::shared_ptr<connection> conn) = 0`

- **Purpose**: Notify application of new client connections.
- **Usage**: Initialize per-connection state, perform authentication, log connections.
- **Called when**: A client successfully connects and the connection object is ready.
- **Connection state**: The connection is fully established and ready for I/O.

Example:

```cpp
void on_connection_opened(std::shared_ptr<connection> conn) override {
    client_sessions[conn->get_fd()] = std::make_unique<Session>();
    send_message(conn, data_buffer("Welcome!\n"));
}
```

#### `on_connection_closed(std::shared_ptr<connection> conn) = 0`

- **Purpose**: Notify application of connection closures.
- **Usage**: Clean up per-connection resources, update statistics, log disconnections.
- **Called when**: Connection is fully closed and cleaned up by the server.
- **Connection state**: The connection is no longer usable for I/O.

Example:

```cpp
void on_connection_closed(std::shared_ptr<connection> conn) override {
    client_sessions.erase(conn->get_fd());
    active_connections_count--;
}
```

#### `on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) = 0`

- **Purpose**: Handle incoming data from clients.
- **Usage**: Parse messages, implement protocol logic, generate responses.
- **Called when**: Complete or partial messages arrive from clients.
- **Data lifetime**: The `data_buffer` is valid during the callback; copy data if needed for asynchronous processing.

Example:

```cpp
void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
    auto request = parse_http_request(db);
    auto response = handle_http_request(request);
    send_message(conn, response);
}
```

#### `on_listen_success() = 0`

- **Purpose**: Notify application that the server has started successfully.
- **Usage**: Log startup, register with service discovery, initialize background tasks.
- **Called when**: The listening socket is bound and the server is ready to accept connections.

#### `on_shutdown_success() = 0`

- **Purpose**: Notify application of successful server shutdown.
- **Usage**: Final cleanup, deregister from service discovery, save persistent state.
- **Called when**: All connections are closed and the server has stopped cleanly.

#### `on_waiting_for_activity() = 0`

- **Purpose**: Hook for idle-time processing.
- **Usage**: Status reporting, health checks, background maintenance tasks.
- **Called when**: The server's I/O loop is waiting for events (implementation-dependent frequency).

## Public virtual methods (control interface)

#### `listen(int timeout = 1000) = 0`

- **Purpose**: Start the server event loop.
- **Behavior**: Typically blocks until `stop_server()` is called or an error occurs.
- **Parameters**: `timeout` - milliseconds for I/O wait operations (epoll_wait, select, etc.).
- **Threading**: Usually runs in the main thread or a dedicated server thread.

#### `stop_server() = 0`

- **Purpose**: Request graceful server shutdown.
- **Behavior**: Signal the event loop to exit cleanly after processing current events.
- **Threading**: Must be safe to call from any thread.

## Usage patterns

### Basic server implementation

```cpp
class EchoServer : public epoll_server {
protected:
    void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
        // Echo received data back to client
        send_message(conn, db);
    }

    void on_connection_opened(std::shared_ptr<connection> conn) override {
        std::cout << "Client connected: " << conn->get_remote_address().to_string() << std::endl;
    }
};

int main() {
    EchoServer server(1024);
    auto listener = make_listener_socket(8080);
    server.register_listener_socket(listener);
    server.listen(); // Blocks until stopped
}
```

### Multi-threaded processing

```cpp
class AsyncServer : public epoll_server {
    ThreadPool pool{4};

protected:
    void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override {
        // Process message asynchronously
        pool.submit([=] {
            auto response = expensive_processing(db);
            send_message(conn, response);
        });
    }
};
```

## Implementation responsibilities

Concrete server classes (like `epoll_server`) must:

1. **Manage listening socket**: Accept incoming connections and create `connection` objects.
2. **Drive I/O loop**: Use platform-specific mechanisms (epoll, select, etc.) to monitor socket events.
3. **Call callbacks**: Invoke the appropriate virtual methods when events occur.
4. **Handle flow control**: Queue outgoing data and manage partial sends/receives.
5. **Provide thread safety**: Ensure the interface can be safely used from multiple threads.

## Notes and best practices

- **Exception safety**: Implementations should catch and handle exceptions in the I/O loop to prevent server crashes.
- **Resource management**: Use RAII and smart pointers for automatic cleanup.
- **Performance**: Consider the threading model carefully - single-threaded event loops vs. thread pools.
- **Scalability**: Design for high connection counts if needed (epoll vs. select, memory usage, etc.).

---

This interface enables building robust, scalable TCP servers while keeping the application logic independent of the underlying I/O mechanism.
