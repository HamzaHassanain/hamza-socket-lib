# Simple C++ Socket Library

A cross-platform C++ socket library built with modern C++17 features. This library provides comprehensive abstractions around low-level socket APIs, including support for both traditional select-based I/O multiplexing and high-performance epoll-based servers on Linux.

## Table of Contents

- [Some Networking Fundamentals](#networking-fundamentals)

- [Prerequisites](#prerequisites)

  - [For Linux (Ubuntu/Debian)](#for-linux-ubuntudebian)
  - [For Linux (CentOS/RHEL/Fedora)](#for-linux-centosrhelfedora)
  - [For Windows](#for-windows)

- [Quick Start](#quick-start)

  - [Using Git Submodules](#using-git-submodules)

- [Building the Project](#building-the-project)

  - [Step 1: Clone the Repository](#step-1-clone-the-repository)
  - [Step 2: Understanding Build Modes](#step-2-understanding-build-modes)
  - [Step 3: Configure Build Mode](#step-3-configure-build-mode)
    - [For Development/Testing](#for-developmenttesting)
    - [For Library Distribution](#for-library-distribution)
  - [Step 4: Build the Project](#step-4-build-the-project)
    - [Option A: Using the Provided Script (Linux/Mac)](#option-a-using-the-provided-script-linuxmac)
    - [Option B: Manual Build (Linux/Mac/Windows)](#option-b-manual-build-linuxmacwindows)
    - [Windows-Specific Build Instructions](#windows-specific-build-instructions)
  - [Step 5: Run the Project](#step-5-run-the-project)
    - [Development Mode (SOCKET_LOCAL_TEST=1)](#development-mode-socket_local_test1)
    - [Library Mode (SOCKET_LOCAL_TEST≠1)](#library-mode-socket_local_test1)
  - [Using the Library in Your Own Project](#using-the-library-in-your-own-project)

- [API Documentation](#api-documentation)

## Some Networking Fundamentals

Understanding the underlying networking concepts is crucial for effectively using this library. Here's an explanation of the key concepts:

### What are Sockets?

**Sockets** are programming abstractions that represent endpoints of network communication. Think of them as "plugs" that allow programs to send and receive data over a network.

```cpp
// Creating a socket in our library
hh_socket::socket server_socket(addr, hh_socket::Protocol::TCP, true);
```

**Key Socket Concepts:**

- **Network Socket**: A software endpoint that establishes bidirectional communication between applications
- **Socket Address**: Combination of IP address and port number that uniquely identifies a network endpoint
- **Socket Types**:
  - **TCP Sockets** (SOCK_STREAM): Reliable, connection-oriented communication
  - **UDP Sockets** (SOCK_DGRAM): Fast, connectionless communication

**Socket Lifecycle:**

1. **Creation**: Allocate socket resources
2. **Binding**: Associate socket with a specific address/port
3. **Listening**: (Server) Wait for incoming connections
4. **Accepting**: (Server) Accept client connections
5. **Connecting**: (Client) Establish connection to server
6. **Data Transfer**: Send/receive data
7. **Closing**: Release socket resources

### What are File Descriptors?

**File Descriptors (FDs)** are integer handles used by the operating system to track open files, sockets, pipes, and other I/O resources.

**Key FD Concepts:**

- **Integer Handle**: Each FD is simply an integer (0, 1, 2, 3, ...)
- **Per-Process**: Each process has its own FD table
- **Standard FDs**:
  - `0` = stdin (standard input)
  - `1` = stdout (standard output)
  - `2` = stderr (standard error)
- **Resource Management**: FDs must be properly closed to prevent resource leaks

**Why FDs Matter for Networking:**

- Sockets are treated as files in Unix-like systems
- The same I/O operations (read/write) work on both files and sockets
- I/O multiplexing (select/poll/epoll) operates on FDs

### How the Kernel Handles Networking

The **kernel** is the core of the operating system that manages hardware resources, including network interfaces.

**Networking in the Kernel:**

1. **Network Stack Layers**:

   ```
   Application Layer  ← Your HTTP Server
   Transport Layer    ← TCP/UDP (our focus)
   Network Layer      ← IP (Internet Protocol)
   Data Link Layer    ← Ethernet/WiFi
   Physical Layer     ← Network hardware
   ```

2. **Kernel's Role**:

   - **Packet Processing**: Receives network packets from hardware
   - **Protocol Implementation**: Handles TCP/IP protocol stack
   - **Socket Management**: Maintains socket state and buffers
   - **I/O Multiplexing**: Efficiently manages multiple connections (If You Implement it)

### What is TCP (Transmission Control Protocol)?

**TCP** is a reliable, connection-oriented transport protocol that ensures data delivery and ordering.

**TCP Characteristics:**

- **Reliable**: Guarantees data delivery through acknowledgments and retransmission
- **Ordered**: Data arrives in the same order it was sent
- **Connection-oriented**: Establishes a connection before data transfer
- **Flow Control**: Manages data transmission speed
- **Error Detection**: Detects and corrects transmission errors

**TCP Connection Process (3-Way Handshake):**

```
Client                    Server
  |                         |
  |-------SYN------------->|  1. Client requests connection
  |<---SYN-ACK-------------|  2. Server acknowledges and responds
  |-------ACK------------->|  3. Client acknowledges, connection established
  |                         |
  |<====Data Transfer=====>|
```

**How Our Library Uses TCP:**

```cpp
// Create TCP socket
hh_socket::socket server_socket(addr, hh_socket::Protocol::TCP, true);

// Listen for connections (server becomes passive)
server_socket.listen();

// Accept incoming connections
auto conn = server_socket.accept();

// Reliable data transfer
conn->send(response_data);
```

## Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

#### For Linux (Ubuntu/Debian):

```bash
# Update package list
sudo apt update

# Install essential build tools
sudo apt install build-essential cmake git

# Verify installations
gcc --version      # Should show GCC 7+ for C++17 support
cmake --version    # Should show CMake 3.10+
git --version      # Any recent version
```

#### For Linux (CentOS/RHEL/Fedora):

```bash
# For CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake git

# For Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git
```

#### For Windows:

1. **Install Git**: Download from [git-scm.com](https://git-scm.com/download/win)
2. **Install CMake**: Download from [cmake.org](https://cmake.org/download/)
3. **Install a C++ Compiler** (choose one):
   - **Visual Studio 2019/2022** (recommended): Download from [visualstudio.microsoft.com](https://visualstudio.microsoft.com/)
   - **MinGW-w64**: Download from [mingw-w64.org](https://www.mingw-w64.org/)
   - **MSYS2**: Download from [msys2.org](https://www.msys2.org/)

## Quick Start

### Using Git Submodules

You just need to clone the repository as a submodule:

```bash
# In your base project directory, run the following command
git submodule add https://github.com/HamzaHassanain/hh-socket-lib.git ./submodules/socket-lib
```

Then in your project's CMakeLists.txt, include the submodule:

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(my_project)

# This block checks for Git and initializes submodules recursively

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")

    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    option(GIT_SUBMODULE_UPDATE_LATEST "Update submodules to latest remote commits" ON)


    if(GIT_SUBMODULE)
        message(STATUS "Initializing and updating submodules...")

        # First, initialize submodules if they don't exist
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_INIT_RESULT)
        if(NOT GIT_SUBMOD_INIT_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_INIT_RESULT}, please checkout submodules")
        endif()

        # If enabled, update submodules to latest remote commits
        if(GIT_SUBMODULE_UPDATE_LATEST)
            message(STATUS "Updating submodules to latest remote commits...")
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --remote --recursive
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_SUBMOD_UPDATE_RESULT)
            if(NOT GIT_SUBMOD_UPDATE_RESULT EQUAL "0")
                message(WARNING "git submodule update --remote --recursive failed with ${GIT_SUBMOD_UPDATE_RESULT}, continuing with current submodule versions")
            else()
                message(STATUS "Submodules updated to latest versions successfully")
            endif()
        endif()
    endif()
endif()


# Add the submodule
add_subdirectory(submodules/socket-lib)

# Link against the library
target_link_libraries(my_project PRIVATE socket_lib)
```

Then in your cpp file, include the socket library header:

```cpp
#include "submodules/socket-lib/socket-lib.hpp"
```

#### Example Simple UDP Server

```cpp
#include "submodules/socket-lib/socket-lib.hpp"
#include <iostream>

int main() {
    try {
        // Create server address and bind UDP socket
        hh_socket::socket_address server_addr(hh_socket::port(8080));
        hh_socket::socket udp_server(server_addr, hh_socket::Protocol::UDP);

        std::cout << "UDP Server listening on port 8080..." << std::endl;

        while (true) {
            // Receive from any client
            hh_socket::socket_address client_addr;
            auto data = udp_server.receive(client_addr);

            std::cout << "Received from " << client_addr.to_string()
                      << ": " << data.to_string() << std::endl;

            // Echo back to sender
            hh_socket::data_buffer response("Echo: " + data.to_string());
            udp_server.send_to(client_addr, response);
        }
    } catch (const std::exception &e) {
        std::cerr << "UDP Server error: " << e.what() << std::endl;
    }
}

```

## Building The Project

This guide will walk you through cloning, building, and running the project on both Linux and Windows systems.

### Step 1: Clone the Repository

Open your terminal (Linux) or Command Prompt/PowerShell (Windows):

```bash
# Clone the repository
git clone https://github.com/HamzaHassanain/hh-socket-lib.git

# Navigate to the project directory
cd hh-socket-lib

# Verify you're in the right directory
ls -la  # Linux/Mac
dir     # Windows CMD
```

### Step 2: Understanding Build Modes

The project supports two build modes controlled by the `.env` file:

#### Development Mode (SOCKET_LOCAL_TEST=1)

- Builds an **executable** for testing
- Includes debugging symbols and AddressSanitizer
- Perfect for learning and development

#### Library Mode (SOCKET_LOCAL_TEST≠1)

- Builds a **static library** for distribution
- Optimized for production use
- Other projects can link against it

### Step 3: Configure Build Mode

Create a `.env` file in the project root:

#### For Development/Testing

```bash
# In .env file, =1 for development mode, and =0 for library mode
SOCKET_LOCAL_TEST=1
```

### Step 4: Build the Project

#### Option A: Using the Provided Script (Linux/Mac)

```bash
# Make the script executable
chmod +x build.sh

# Run the build script
./build.sh
```

This script automatically:

1. Creates a `build` directory
2. Runs CMake configuration
3. Compiles the project
4. Runs the executable (if in development mode)

#### Option B: Manual Build (Linux/Mac/Windows)

```bash
# Create build directory
mkdir -p build

# Configure with CMake
cmake -S . -B build

# Build the project
cd build
make -j$(nproc)

# Back to project root
cd ..

```

#### Windows-Specific Build Instructions

**Using Visual Studio:**

```cmd
# Open Developer Command Prompt for Visual Studio
mkdir build
cd build
cmake ..
cmake --build . --config Release

```

**Using MinGW:**

```cmd
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### Step 5: Run the Project

#### Development Mode (SOCKET_LOCAL_TEST=1):

```bash
# Linux/Mac
./build/socket_lib

# Windows
.\build\Debug\socket_lib.exe   # Debug build
.\build\Release\socket_lib.exe # Release build
```

#### Library Mode (SOCKET_LOCAL_TEST):

The build will create a static library file:

- **Linux/Mac**: `build/libsocket_lib.a`
- **Windows**: `build/socket_lib.lib` or `build/Debug/socket_lib.lib`

### Using the Library in Your Own Project

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(my_project)

# Find the library
find_library(SOCKET_LIB
    NAMES socket_lib
    PATHS /path/to/hh-socket-lib/build
)

# include "path-to-hh-socket-lib/socket-lib.hpp" in your cpp file for the full library
# or un-comment the bellow line
# target_include_directories(my_app PRIVATE /path/to/hh-socket-lib/includes)

# Link against the library
add_executable(my_app main.cpp)
target_link_libraries(my_app ${SOCKET_LIB})
```

## API Documentation

Below is a reference of the core public classes and their commonly used methods. Include the corresponding header before use.

### For a more comprehensive guide:

- [file_descriptor](docs/file_descriptor.md)
- [port](docs/port.md)
- [famaily](docs/famaily.md)
- [ip_address](docs/ip_address.md)
- [data_buffer](docs/data_buffer.md)
- [sokcet_address](docs/sokcet_address.md)
- [exceptions](docs/exceptions.md)`
- [socket](docs/socket.md)
- [connection](docs/connection.md)
- [tcp_server](docs/tcp_server.md)
- [epoll_server](docs/epoll_server.md)
- [utilities](docs/utilities.md)

### hh_socket::file_descriptor

```cpp
#include "file_descriptor.h"

// - Purpose: RAII wrapper for platform socket handles / file descriptors.
// - Key constructors:
  file_descriptor() // — creates invalid descriptor
  explicit file_descriptor(socket_t fd) // — wrap an existing descriptor
  // - Move-only: move ctor/assignment available, copy operations deleted
// - Important methods:
  int get() const  //— raw descriptor value (or INVALID_SOCKET_VALUE)
  bool is_valid() const
  void invalidate() const // resets the file descriptor to invalid socket value
  // - comparison operators: `operator==`, `operator!=`, `operator<`
  friend std::ostream &operator<<(std::ostream &os, const file_descriptor &fd);
```

### hh_socket::ip_address

```cpp
#include "ip_address.h"

// - Purpose: Simple string wrapper for IPv4/IPv6 addresses.
// - Constructors:
  explicit ip_address()
  explicit ip_address(const std::string &address)
// - Methods/operators:
  - `const std::string &get() const`
  // - `operator==`, `operator!=`, `operator<`
  friend std::ostream &operator<<(std::ostream &os, const ip_address &ip);
```

### hh_socket::port

```cpp
#include "port.h"

// - Purpose: Type-safe port number wrapper with range validation (0–65535).
// - Constructors:
  explicit port()
  explicit port(int id) // — validates range, throws on invalid value
// - Methods/operators:
  int get() const
  // - `operator==`, `operator!=`, `operator<`
  friend std::ostream &operator<<(std::ostream &os, const port &p);
```

### hh_socket::family

```cpp
#include "family.h"
// - Purpose: Type-safe address family wrapper (IPv4 / IPv6).
// - Constructors:
  explicit family() // — defaults to IPv4
  explicit family(int id) // — accepts IPV4 or IPV6
// - Methods/operators:
  int get() const
  // - `operator==`, `operator!=`, `operator<`
  friend std::ostream &operator<<(std::ostream &os, const family &f);
```

### hh_socket::socket_address

```cpp
#include "socket_address.hpp"

// - Purpose: Combines `ip_address`, `port`, and `family` and exposes a system `sockaddr` for syscalls.
// - Key constructors:
  explicit socket_address()
  explicit socket_address(const port &port_id, const ip_address &address = ip_address("0.0.0.0"), const family &family_id = family(IPV4))
  explicit socket_address(sockaddr_storage &addr)
  // - copy / move constructors and assignments supported
// - Important methods:
  ip_address get_ip_address() const
  port get_port() const
  family get_family() const
  std::string to_string() const // — human-readable "IP:port"
  sockaddr *get_sock_addr() const // — pointer suitable for `bind()` / `connect()`
  socklen_t get_sock_addr_len() const
```

### hh_socket::data_buffer

```cpp
#include "data_buffer.hpp"

// - Purpose: Dynamic buffer for storing and managing binary data with automatic resizing.
// - Key constructors:
  explicit data_buffer() // — empty buffer
  explicit data_buffer(const std::string &str) // — from string
  explicit data_buffer(const char *data, std::size_t size) // — from raw data
  // - copy and move constructors/assignments supported
// - Data manipulation:
  void append(const char *data, std::size_t size) // — append raw data
  void append(const std::string &str) // — append string
  void clear() // — remove all data
// - Data access:
  const char *data() const // — pointer to raw data
  std::size_t size() const // — size in bytes
  bool empty() const // — check if empty
  std::string to_string() const // — convert to string
```

### hh_socket::socket_exception

```cpp
#include "exceptions.hpp"

// - Purpose: Base exception class for all socket-related errors with detailed error information.
// - Constructor:
  explicit socket_exception(const std::string &message, const std::string &type, const std::string &thrower_function = "SOCKET_FUNCTION")
// - Error information:
  virtual const char *type() const noexcept // — exception type (e.g., "SocketCreation", "SocketBinding")
  virtual const char *thrower_function() const noexcept // — function that threw the exception
  virtual const char *what() const noexcept override // — formatted error message
```

### hh_socket::socket

```cpp
#include "socket.hpp"

// - Purpose: Cross-platform socket wrapper for TCP and UDP network operations with resource management.
// - Key constructors:
  explicit socket(const Protocol &protocol) // — create unbound socket
  explicit socket(const socket_address &addr, const Protocol &protocol) // — create and bind
  // - Move-only: copy operations deleted, move operations available
// - Socket setup methods:
  void bind(const socket_address &addr)
  void set_reuse_address(bool reuse)
  void set_non_blocking(bool enable)
  void set_close_on_exec(bool enable)
  void set_option(int level, int optname, int optval) // — custom socket options
// - TCP connection methods:
  void connect(const socket_address &addr) // — client connect
  void listen(int backlog = SOMAXCONN) // — server listen
  std::shared_ptr<connection> accept(bool NON_BLOCKING = false) // — accept connection
// - UDP communication methods:
  data_buffer receive(socket_address &client_addr) // — receive from any client
  void send_to(const socket_address &addr, const data_buffer &data) // — send to specific address
// - General methods:
  socket_address get_bound_address() const
  int get_fd() const // — raw file descriptor
  void disconnect() // — close socket
  bool is_connected() const
  bool operator<(const socket &other) const // — for container ordering
```

### hh_socket::connection

```cpp
#include "connection.hpp"

// - Purpose: Represents an established TCP connection with send/receive capabilities.
// - Key constructor:
  connection(file_descriptor fd, const socket_address &local_addr, const socket_address &remote_addr)
  // - Move-only: copy operations deleted, move operations available
// - Communication methods:
  ssize_t send(const data_buffer &data) // — send data, returns bytes sent
  data_buffer receive() // — receive data from connection
// - Connection management:
  void close() // — close the connection
  bool is_connection_open() const
// - Address information:
  int get_fd() const // — raw file descriptor
  socket_address get_remote_address() const
  socket_address get_local_address() const
```

### hh_socket::tcp_server

```cpp
#include "tcp_server.hpp"

// - Purpose: Abstract base class for TCP server implementations with event-driven callbacks.
// - Key characteristics:
  // - Pure virtual class (cannot be instantiated directly)
  // - Provides interface for connection and message handling
  // - Implemented by select_server and epoll_server
// - Pure virtual methods to implement:
  virtual void close_connection(std::shared_ptr<connection> conn) = 0 // — request connection closure
  virtual void send_message(std::shared_ptr<connection> conn, const data_buffer &db) = 0 // — send data to connection
  virtual void on_exception_occurred(const std::exception &e) = 0 // — handle server exceptions
  virtual void on_connection_opened(std::shared_ptr<connection> conn) = 0 // — new connection callback
  virtual void on_connection_closed(std::shared_ptr<connection> conn) = 0 // — connection closed callback
  virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) = 0 // — data received callback
  virtual void on_listen_success() = 0 // — server started successfully
  virtual void on_shutdown_success() = 0 // — server shutdown completed
  virtual void on_waiting_for_activity() = 0 // — server waiting for events
// - Server control methods:
  virtual void listen(int timeout = 1000) = 0 // — start server event loop
  virtual void stop_server() = 0 // — request graceful shutdown
```

### hh_socket::epoll_server

```cpp
#include "epoll_server.hpp"

// - Purpose: High-performance Linux epoll-based TCP server for thousands of concurrent connections.
// - Platform support: Linux only (requires epoll system call)
// - Key constructor:
  epoll_server(int max_fds) // — specify number of maximum open file descriptors (more files, means more memory usage)
// - Server management:
  virtual void listen(int timeout) override // — start epoll event loop
  virtual bool register_listener_socket(std::shared_ptr<socket> sock_ptr) // — register listening socket
  virtual void stop_server() override // — graceful shutdown
// - Connection interface (inherit from tcp_server):
  void close_connection(std::shared_ptr<connection> conn) override // — close specific connection
  void send_message(std::shared_ptr<connection> conn, const data_buffer &db) override // — send data asynchronously
// - Event callbacks to override:
  virtual void on_connection_opened(std::shared_ptr<connection> conn) override
  virtual void on_connection_closed(std::shared_ptr<connection> conn) override
  virtual void on_message_received(std::shared_ptr<connection> conn, const data_buffer &db) override
  virtual void on_exception_occurred(const std::exception &e) override
  virtual void on_listen_success() override
  virtual void on_shutdown_success() override
  virtual void on_waiting_for_activity() override
// - Performance features:
  // - Edge-triggered epoll for O(1) event notification
  // - Efficient batch processing of events
  // - Non-blocking I/O throughout
  // - Automatic write buffering and flow control
```

### hh_socket::utilities

```cpp
#include "utilities.hpp"

// - Purpose: Cross-platform socket utilities and helper functions for network programming.
// - Key constants:
  const int IPV4 = AF_INET // — IPv4 address family identifier
  const int IPV6 = AF_INET6 // — IPv6 address family identifier
  const int MIN_PORT = 1024 // — Minimum valid port number (reserved)
  const int MAX_PORT = 65535 // — Maximum valid port number
  const std::size_t DEFAULT_BUFFER_SIZE = 4096 // — Default buffer size for socket I/O
  const std::size_t MAX_BUFFER_SIZE = 65536 // — Maximum buffer size for single operations
  const int DEFAULT_TIMEOUT = 5000 // — Default socket timeout (milliseconds)
  const int CONNECT_TIMEOUT = 10000 // — Connection establishment timeout
  const int RECV_TIMEOUT = 10000 // — Receive operation timeout
  const int DEFAULT_LISTEN_BACKLOG = SOMAXCONN // — Default listen queue size

// - Protocol enumeration:
  enum class Protocol {
    TCP = SOCK_STREAM, // — Transmission Control Protocol (reliable, connection-oriented)
    UDP = SOCK_DGRAM   // — User Datagram Protocol (unreliable, connectionless)
  }

// - Network address conversion utilities:
  void convert_ip_address_to_network_order(const family &family_ip, const ip_address &address, void *addr)
  // — Convert IP address string to network byte order using inet_pton()/InetPtonA()
  std::string get_ip_address_from_network_address(sockaddr_storage &addr)
  // — Extract IP address string from network address structure using inet_ntop()

// - Port management utilities:
  port get_random_free_port()
  // — Generate random available port number in range 1024-65535 (thread-safe)
  bool is_valid_port(port p)
  // — Validate port number range (1-65535), does not check availability
  bool is_free_port(port p)
  // — Check if port is currently available for binding (platform-specific)

// - Byte order conversion utilities:
  int convert_host_to_network_order(int port)
  // — Convert port from host to network byte order using htons()
  int convert_network_order_to_host(int port)
  // — Convert port from network to host byte order using ntohs()

// - Cross-platform socket management:
  bool initialize_socket_library()
  // — Initialize socket library (Windows: WSAStartup, Unix/Linux: no-op)
  void cleanup_socket_library()
  // — Cleanup socket library (Windows: WSACleanup, Unix/Linux: no-op)
  void close_socket(socket_t socket)
  // — Close socket using platform-appropriate function (closesocket/close)

// - Socket validation functions:
  bool is_valid_socket(socket_t socket)
  // — Check if socket handle is valid (Windows: != INVALID_SOCKET, Unix: >= 0)
  bool is_socket_open(int fd)
  // — Check if file descriptor represents an open socket using getsockopt()
  bool is_socket_connected(socket_t socket)
  // — Check if socket is currently connected using SO_ERROR and getpeername()

// - Utility functions:
  std::string get_error_message()
  // — Get platform-specific error message for last socket operation
  std::string to_upper_case(const std::string &input)
  // — Convert string to uppercase (returns new string, original unchanged)

// - High-level socket creation:
  std::shared_ptr<hh_socket::socket> make_listener_socket(uint16_t port, const std::string &ip = "0.0.0.0", int backlog = SOMAXCONN)
  // — Create a ready-to-use TCP listener socket bound to specified address and port
```

### Usage Examples

**1. Basic TCP Echo Server with epoll (Linux):**

```cpp

#include "includes/epoll_server.hpp"
#include "includes/socket.hpp"
#include "includes/socket_address.hpp"
#include "includes/data_buffer.hpp"
#include "includes/utilities.hpp"

#include <iostream>

class EchoServer : public hh_socket::epoll_server {
public:
    EchoServer() : hh_socket::epoll_server(1000) {} // Max 1000 connections

protected:
    void on_connection_opened(std::shared_ptr<hh_socket::connection> conn) override {
        std::cout << "Client connected from: " << conn->get_remote_address().to_string() << std::endl;
    }

    void on_message_received(std::shared_ptr<hh_socket::connection> conn,
                           const hh_socket::data_buffer &message) override {
        std::cout << "Received: " << message.to_string() << std::endl;
        // Echo the message back
        send_message(conn, message);
        close_connection(conn);
    }

    void on_connection_closed(std::shared_ptr<hh_socket::connection> conn) override {
        std::cout << "Client disconnected: " << conn->get_remote_address().to_string() << std::endl;
    }

    void on_exception_occurred(const std::exception &e) override {
        std::cerr << "Server error: " << e.what() << std::endl;
    }

    void on_listen_success() override {
        std::cout << "Echo server started successfully!" << std::endl;
    }

    void on_shutdown_success() override {
        std::cout << "Server shutdown complete." << std::endl;
    }

    void on_waiting_for_activity() override {
        // Optional: periodic maintenance tasks
    }
};

int main() {
    try {
        // Create server address (bind to all interfaces on port 8080)
        hh_socket::socket_address server_addr(
            hh_socket::port(8080),
            hh_socket::ip_address("0.0.0.0")
        );

        // Create TCP listening socket
        auto listener = hh_socket::make_listener_socket(8080);

        // Create and run the echo server
        EchoServer server;
        if (server.register_listener_socket(listener)) {
            server.listen(); // Start the server event loop
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

**2. Simple TCP Client:**

```cpp
#include "includes/socket.hpp"
#include "includes/socket_address.hpp"
#include "includes/data_buffer.hpp"
#include <iostream>
#include <string>

int main() {
    try {
        // Create client socket
        hh_socket::socket client(hh_socket::Protocol::TCP);

        // Connect to server
        hh_socket::socket_address server_addr(
            hh_socket::port(8080),
            hh_socket::ip_address("127.0.0.1")
        );
        client.connect(server_addr);

        // Accept a connection to get connection object
        auto conn = client.accept(); // This creates a connection wrapper

        // Send a message
        hh_socket::data_buffer message("Hello, Server!");
        conn->send(message);

        // Receive response
        auto response = conn->receive();
        std::cout << "Server response: " << response.to_string() << std::endl;

        conn->close();
    } catch (const std::exception &e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

**3. UDP Client/Server Example:**

```cpp
#include "includes/socket.hpp"
#include "includes/socket_address.hpp"
#include "includes/data_buffer.hpp"
#include <iostream>

// UDP Server
void udp_server_example() {
    try {
        // Create server address and bind UDP socket
        hh_socket::socket_address server_addr(hh_socket::port(8080));
        hh_socket::socket udp_server(server_addr, hh_socket::Protocol::UDP);

        std::cout << "UDP Server listening on port 8080..." << std::endl;

        while (true) {
            // Receive from any client
            hh_socket::socket_address client_addr;
            auto data = udp_server.receive(client_addr);

            std::cout << "Received from " << client_addr.to_string()
                      << ": " << data.to_string() << std::endl;

            // Echo back to sender
            hh_socket::data_buffer response("Echo: " + data.to_string());
            udp_server.send_to(client_addr, response);
        }
    } catch (const std::exception &e) {
        std::cerr << "UDP Server error: " << e.what() << std::endl;
    }
}

// UDP Client
void udp_client_example() {
    try {
        // Create UDP client socket
        hh_socket::socket udp_client(hh_socket::Protocol::UDP);

        // Define server address
        hh_socket::socket_address server_addr(
            hh_socket::port(8080),
            hh_socket::ip_address("127.0.0.1")
        );

        // Send message
        hh_socket::data_buffer message("Hello UDP Server!");
        udp_client.send_to(server_addr, message);

        // Receive response
        hh_socket::socket_address response_addr;
        auto response = udp_client.receive(response_addr);
        std::cout << "Server response: " << response.to_string() << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "UDP Client error: " << e.what() << std::endl;
    }
}
```

**4. Basic TCP Server using Raw Socket (Cross-platform):**

```cpp
#include "includes/socket.hpp"
#include "includes/socket_address.hpp"
#include "includes/connection.hpp"
#include <iostream>
#include <thread>

void handle_client(std::shared_ptr<hh_socket::connection> conn)
{
    try
    {
        std::cout << "Handling client: " << conn->get_remote_address().to_string() << std::endl;

        while (conn->is_connection_open())
        {
            // Receive data from client
            auto data = conn->receive();
            std::cout << "Received: " << data.to_string() << std::endl;
            if (data.empty())
                break; // Client disconnected

            // Echo back with prefix
            hh_socket::data_buffer response("Echo: " + data.to_string());
            conn->send(response);
        }

        conn->close();
        std::cout << "Client disconnected." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client handling error: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        // Create and bind server socket
        hh_socket::socket_address server_addr(
            hh_socket::port(8000),
            hh_socket::ip_address("127.0.0.1"));
        hh_socket::socket server(hh_socket::Protocol::TCP);
        server.set_reuse_address(true);
        server.bind(server_addr);

        server.listen();

        std::cout << "TCP Server listening on " << server_addr.to_string() << std::endl;

        while (true)
        {
            // Accept incoming connections
            auto client_conn = server.accept();

            // Handle each client in a separate thread
            std::thread client_thread(handle_client, client_conn);
            client_thread.detach(); // Let thread run independently
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

**5. File Transfer Client:**

```cpp
#include "includes/socket.hpp"
#include "includes/socket_address.hpp"
#include "includes/data_buffer.hpp"
#include <iostream>
#include <fstream>

bool send_file(const std::string &filename, const std::string &server_ip, int port) {
    try {
        // Open file
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << filename << std::endl;
            return false;
        }

        // Connect to server
        hh_socket::socket client(hh_socket::Protocol::TCP);
        hh_socket::socket_address server_addr(
            hh_socket::port(port),
            hh_socket::ip_address(server_ip)
        );
        client.connect(server_addr);
        auto conn = client.accept();

        // Send filename first
        hh_socket::data_buffer filename_msg(filename);
        conn->send(filename_msg);

        // Send file contents in chunks
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            hh_socket::data_buffer chunk(buffer, file.gcount());
            conn->send(chunk);
        }

        std::cout << "File sent successfully: " << filename << std::endl;
        conn->close();
        return true;

    } catch (const std::exception &e) {
        std::cerr << "File transfer error: " << e.what() << std::endl;
        return false;
    }
}
```
