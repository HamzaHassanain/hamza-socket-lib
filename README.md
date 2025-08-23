# Simple C++ Socket Library

A high-performance, cross-platform C++ socket library built with modern C++17 features. This library provides comprehensive abstractions around low-level socket APIs, including support for both traditional select-based I/O multiplexing and high-performance epoll-based servers on Linux.

## Table of Contents

- [Some Networking Fundamentals](#networking-fundamentals)
- [Building the Project](#building-the-project)

  - [Prerequisites](#prerequisites)
  - [How to Build](#how-to-build)

    - [Prerequisites](#prerequisites-1)
      - [For Linux (Ubuntu/Debian)](#for-linux-ubuntudebian)
      - [For Linux (CentOS/RHEL/Fedora)](#for-linux-centosrhelfedora)
      - [For Windows](#for-windows)
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
    - [Project Structure After Build](#project-structure-after-build)
    - [Using the Library in Your Own Project](#using-the-library-in-your-own-project)

- [API Documentation](#api-documentation)

## Some Networking Fundamentals

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
hamza_socket::socket server_socket(addr, hamza_socket::Protocol::TCP, true);

// Listen for connections (server becomes passive)
server_socket.listen();

// Accept incoming connections
auto conn = server_socket.accept();

// Reliable data transfer
conn.send(response_data);
```

## Building The Project

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
git clone https://github.com/HamzaHassanain/hamza-socket-lib.git

# Navigate to the project directory
cd hamza-socket-lib

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
# Create .env file for development mode
echo "SOCKET_LOCAL_TEST=1" > .env

# On Windows (PowerShell):
echo "SOCKET_LOCAL_TEST=1" | Out-File -FilePath .env -Encoding ASCII

# On Windows (CMD):
echo SOCKET_LOCAL_TEST=1 > .env
```

#### For Library Distribution:

```bash
# Create .env file for library mode (or leave empty)
echo "SOCKET_LOCAL_TEST=0" > .env
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

### Project Structure After Build

```
hamza-http-server-lib/
├── build/                     # Build artifacts
│   ├── socket_lib           # Executable (Linux, development mode)
│   ├── socket_lib.exe       # Executable (Windows, development mode)
│   ├── libsocket_lib.a      # Static library (Linux, library mode)
│   └── socket_lib.lib       # Static library (Windows, library mode)
├── src/                      # Source files
├── includes/                 # Header files
├── app.cpp                   # Example application
├── CMakeLists.txt           # Build configuration
├── .env                     # Build mode configuration
└── build.sh                   # Build script (Linux/Mac)
```

### Using the Library in Your Own Project

Once built in library mode, you can use it in other projects:

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(my_project)

# Find the library
find_library(HAMZA_SOCKET_LIB
    NAMES socket_lib
    PATHS /path/to/hamza-socket-lib/build
)

# Link against the library
add_executable(my_app main.cpp)
target_link_libraries(my_app ${HAMZA_SOCKET_LIB})
target_include_directories(my_app PRIVATE /path/to/hamza-socket-lib/includes)
```

## API Documentation

Below is a reference of the core public classes and their commonly used methods. Include the corresponding header before use, for example:

### hamza_socket::file_descriptor

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
  void invalidate() const
  // - comparison operators: `operator==`, `operator!=`, `operator<`
  friend std::ostream &operator<<(std::ostream &os, const file_descriptor &fd);
```

### hamza_socket::ip_address

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

### hamza_socket::port

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

### hamza_socket::family

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

### hamza_socket::socket_address

```cpp
#include "socket_address.h"

// - Purpose: Combines `ip_address`, `port`, and `family` and exposes a system `sockaddr` for syscalls.
// - Key constructors:
  explicit socket_address()
  explicit socket_address(const port &port_id, const ip_address &address = ip_address("0.0.0.0"), const family &family_id = family(IPV4))
  explicit socket_address(sockaddr_storage &addr)`
  // - copy / move constructors and assignments supported
// - Important methods:
  ip_address get_ip_address() const
  port get_port() const
  family get_family() const
  std::string to_string() const // — human-readable "IP:port"
  sockaddr *get_sock_addr() const // — pointer suitable for `bind()` / `connect()`
  socklen_t get_sock_addr_len() const
```

### Something next
