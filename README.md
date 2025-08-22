# hamza_socket HTTP Server Library

A high-performance, cross-platform HTTP server library written in C++17. This library provides a complete networking stack with TCP server capabilities, socket management, and HTTP protocol handling.

## Table of Contents

- [Overview](#overview)

- [Fundamental Networking Concepts](#fundamental-networking-concepts)
- [Quick Start application](#quick-start-application)
- [Building the Project](#building-the-project)

  - [Prerequisites](#prerequisites)
  - [How to Build](#how-to-build)
    - [Prerequisites](#prerequisites-1)
      - [For Linux (Ubuntu/Debian)](#for-linux-ubuntudebian)
      - [For Linux (CentOS/RHEL/Fedora)](#for-linux-centosrhelfedora)
      - [For Windows](#for-windows)
    - [Step 1: Clone the Repository](#step-1-clone-the-repository)
    - [Step 2: Understanding Build Modes](#step-2-understanding-build-modes)
      - [Development Mode (LOCAL_TEST=1)](#development-mode-local_test1)
      - [Library Mode (LOCAL_TEST≠1)](#library-mode-local_test1)
    - [Step 3: Configure Build Mode](#step-3-configure-build-mode)
      - [For Development/Testing](#for-developmenttesting)
      - [For Library Distribution](#for-library-distribution)
    - [Step 4: Build the Project](#step-4-build-the-project)
      - [Option A: Using the Provided Script (Linux/Mac)](#option-a-using-the-provided-script-linuxmac)
      - [Option B: Manual Build (Linux/Mac/Windows)](#option-b-manual-build-linuxmacwindows)
      - [Windows-Specific Build Instructions](#windows-specific-build-instructions)
    - [Step 5: Run the Project](#step-5-run-the-project)
      - [Development Mode (LOCAL_TEST=1)](#development-mode-local_test1-1)
      - [Library Mode (LOCAL_TEST≠1)](#library-mode-local_test1-1)
    - [Project Structure After Build](#project-structure-after-build)
    - [Using the Library in Your Own Project](#using-the-library-in-your-own-project)

## Overview

This library provides a modern C++17 implementation of an HTTP server built on fundamental networking principles. It abstracts away the complexity of low-level socket programming while providing full control over HTTP request/response handling. The library implements a complete networking stack from raw sockets up to HTTP protocol handling, making it suitable for both educational purposes and production applications.

## Fundamental Networking Concepts

Understanding the underlying networking concepts is crucial for effectively using this library. Here's an explanation of the key concepts:

### What are Sockets?

**Sockets** are programming abstractions that represent endpoints of network communication. Think of them as "plugs" that allow programs to send and receive data over a network.

```cpp
// Creating a socket in our library
hamza_socket::socket server_socket(addr, hamza_socket::Protocol::TCP, true);
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
   - **I/O Multiplexing**: Efficiently manages multiple connections

3. **How Our Library Interacts with Kernel**:

   ```cpp
   // When you call this:
   socket.send_on_connection(data);

   // The kernel:
   // 1. Copies data to kernel buffer
   // 2. Breaks data into TCP segments
   // 3. Adds TCP/IP headers
   // 4. Sends packets via network interface
   ```

**I/O Multiplexing with `select()`:**
Our library uses `select()` to efficiently handle multiple client connections:

```cpp
// Our select_server manages multiple file descriptors
hamza_socket::select_server fd_select_server;
fd_select_server.add_fd(client_fd);
int activity = fd_select_server.select(); // Kernel checks all FDs for activity
```

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
hamza_socket::socket server_socket(addr, hamza_socket::Protocol::TCP, true);

// Listen for connections (server becomes passive)
server_socket.listen();

// Accept incoming connections
auto client_socket = server_socket.accept();

// Reliable data transfer
client_socket.send_on_connection(response_data);
```

**The tcp_server abstract class**

### What is HTTP (HyperText Transfer Protocol)?

**HTTP** is an application-layer protocol built on top of TCP for transferring web content.

**HTTP Characteristics:**

- **Request-Response**: Client sends request, server sends response
- **Stateless**: Each request is independent
- **Text-based**: Human-readable headers and methods
- **Flexible**: Supports various content types and methods

**HTTP Message Structure:**

```
HTTP Request:
GET /path HTTP/1.1          ← Request Line
Host: example.com           ← Headers
Content-Type: text/html
                           ← Empty line
[optional body]            ← Body

HTTP Response:
HTTP/1.1 200 OK            ← Status Line
Content-Type: text/html    ← Headers
Content-Length: 13
                           ← Empty line
Hello, World!              ← Body
```

**How Our Library Handles HTTP:**

1. **Raw TCP Data Reception**:

   ```cpp
   // TCP layer receives raw bytes
   // then signals the derived class of an incomming message from the connection, gives it a pointer to the socket that sends the message
   hamza_socket::data_buffer raw_data = socket.receive_on_connection();
   this->on_message_received(sock_ptr, raw_data);
   ```

2. **HTTP Request Parsing**:

   since the http server is just a derived class of the main tcp server, it is responsible for parsing the raw data, and making sure
   that the full http request is received properly, that all is done in the `on_message_received` method.

   ```cpp
   void http_server::on_message_received(std::shared_ptr<hamza_socket::socket> sock_ptr, const hamza_socket::data_buffer &message)
   {
       // Parse the raw TCP data into HTTP components
       auto handled_data = handler.handle(sock_ptr, message);

       if (!handled_data.is_complete())
           return; // Wait for more data across multiple TCP packets

       // Create HTTP request/response objects and invoke user callback
       http_request request(handled_data.method, handled_data.uri, handled_data.version,
                           handled_data.headers, handled_data.body, sock_ptr);
       http_response response("HTTP/1.1", {}, sock_ptr);
       this->on_request_received(request, response);
   }
   ```

   **HTTP Parsing Features:**

   - **Stateful Parsing**: Handles HTTP requests split across multiple TCP packets
   - **Thread-Safe**: Concurrent request parsing with mutex protection
   - **Multiple Encoding Support**:
     - **Content-Length**: Fixed-size request bodies
     - **Chunked Transfer**: Variable-size streaming bodies
   - **RFC Compliance**: Proper HTTP/1.1 header parsing and validation
   - **Memory Efficient**: Automatic cleanup of completed requests
   - **Error Resilient**: Graceful handling of malformed input

   **Parse Flow:**

   1. **Request Line**: Extract HTTP method, URI path, and version
   2. **Headers**: Parse all headers into a multimap (supports duplicates)
   3. **Body Length Detection**: Determine body size via Content-Length or chunked encoding
   4. **Body Accumulation**: Collect body data across multiple TCP packets if needed
   5. **Completion Check**: Return complete HTTP request when all data received

   The parser maintains per-client state using the remote socket address as a unique key, allowing
   concurrent handling of multiple incomplete requests from different clients.

   ```

   ```

3. **HTTP Response Building**:

   ```cpp
   // Your application logic
   response.set_status(200, "OK");
   response.add_header("Content-Type", "application/json");
   response.set_body("{\"message\": \"Hello\"}");
   ```

4. **HTTP Response Transmission**:

   ```cpp
   // Library converts response to raw TCP data and sends

   response.send(); // Formats and sends via TCP socket
   response.end(); // ends the connection, and releases resources
   ```

**Complete Flow in Our Library:**

```
1. Kernel receives TCP packet
2. Kernel notifies select() that FD has data
3. Our library reads raw bytes from socket
4. HTTP parser converts bytes to http_request object
5. Your callback function handles the request
6. You build http_response object
7. Library converts response to raw bytes
8. TCP socket sends bytes back to client
9. Kernel transmits packets over network
```

### Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Standard networking libraries (automatically linked)

### How to build

This guide will walk you through cloning, building, and running the project on both Linux and Windows systems.

### Prerequisites

Before you start, ensure you have the following installed:

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

### Step 1: Clone the Repository

Open your terminal (Linux) or Command Prompt/PowerShell (Windows):

```bash
# Clone the repository
git clone https://github.com/hamza_socketHassanain/hamza_socket-http-server-lib.git

# Navigate to the project directory
cd hamza_socket-http-server-lib

# Verify you're in the right directory
ls -la  # Linux/Mac
dir     # Windows CMD
```

### Step 2: Understanding Build Modes

The project supports two build modes controlled by the `.env` file:

#### Development Mode (LOCAL_TEST=1)

- Builds an **executable** for testing
- Includes debugging symbols and AddressSanitizer
- Perfect for learning and development

#### Library Mode (LOCAL_TEST≠1)

- Builds a **static library** for distribution
- Optimized for production use
- Other projects can link against it

### Step 3: Configure Build Mode

Create a `.env` file in the project root:

#### For Development/Testing

```bash
# Create .env file for development mode
echo "LOCAL_TEST=1" > .env

# On Windows (PowerShell):
echo "LOCAL_TEST=1" | Out-File -FilePath .env -Encoding ASCII

# On Windows (CMD):
echo LOCAL_TEST=1 > .env
```

#### For Library Distribution:

```bash
# Create .env file for library mode (or leave empty)
echo "LOCAL_TEST=0" > .env
```

### Step 4: Build the Project

#### Option A: Using the Provided Script (Linux/Mac)

```bash
# Make the script executable
chmod +x run.sh

# Run the build script
./run.sh
```

This script automatically:

1. Creates a `build` directory
2. Runs CMake configuration
3. Compiles the project
4. Runs the executable (if in development mode)

#### Option B: Manual Build (Linux/Mac/Windows)

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build .

# Alternative: use make on Linux/Mac
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # Mac
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

#### Development Mode (LOCAL_TEST=1):

```bash
# Linux/Mac
./build/http_server

# Windows
.\build\Debug\http_server.exe   # Debug build
.\build\Release\http_server.exe # Release build
```

#### Library Mode (LOCAL_TEST≠1):

The build will create a static library file:

- **Linux/Mac**: `build/libhttp_server.a`
- **Windows**: `build/http_server.lib` or `build/Debug/http_server.lib`

### Project Structure After Build

```
hamza_socket-http-server-lib/
├── build/                     # Build artifacts
│   ├── http_server           # Executable (Linux, development mode)
│   ├── http_server.exe       # Executable (Windows, development mode)
│   ├── libhttp_server.a      # Static library (Linux, library mode)
│   └── http_server.lib       # Static library (Windows, library mode)
├── src/                      # Source files
├── includes/                 # Header files
├── app.cpp                   # Example application
├── CMakeLists.txt           # Build configuration
├── .env                     # Build mode configuration
└── run.sh                   # Build script (Linux/Mac)
```

### Using the Library in Your Own Project

Once built in library mode, you can use it in other projects:

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(my_project)

# Find the library
find_library(hamza_socket_HTTP_LIB
    NAMES http_server
    PATHS /path/to/hamza_socket-http-server-lib/build
)

# Link against the library
add_executable(my_app main.cpp)
target_link_libraries(my_app ${hamza_socket_HTTP_LIB})
target_include_directories(my_app PRIVATE /path/to/hamza_socket-http-server-lib/includes)
```
