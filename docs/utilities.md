# utilities (Platform helpers and network utilities)

Source: `includes/utilities.hpp` and `src/utilities.cpp`

This document explains the purpose, behaviour and implementation details of the utility layer used across the library. The utilities provide:

- Cross-platform socket type aliases and sentinel values
- Common constants (address families, port ranges, buffer sizes)
- Platform initialization/cleanup for sockets (Windows)
- Convenience helpers for byte-order conversion and textual/binary IP conversion
- Port helpers (random free port search and availability checks)

The implementation contains real network operations (bind checks) and platform-specific code paths; read the notes and warnings about race conditions before using the helpers in production.

## Platform aliases and constants

- `socket_t` — alias for the OS socket type:

  - Unix: `int`
  - Windows: `SOCKET`

- `INVALID_SOCKET_VALUE` — sentinel invalid socket value: `-1` (Unix) or `INVALID_SOCKET` (Windows).

- `SOCKET_ERROR_VALUE` — socket error sentinel: `-1` (Unix) or `SOCKET_ERROR` (Windows).

- `IPV4`, `IPV6` — mapped to OS constants `AF_INET` and `AF_INET6`.

- `MIN_PORT = 1024`, `MAX_PORT = 65535` — library port range used for helper functions and random port selection.

- `DEFAULT_BUFFER_SIZE` — recommended default buffer size for socket I/O (4 KiB).

## Implementation notes and common variables

- `get_random_free_port_mutex` — a global `std::mutex` used to make random-port generation and checking thread-safe.

- Random generator inside `get_random_free_port()`

  - `std::mt19937` seeded with a steady-clock timestamp for good entropy.
  - `std::uniform_int_distribution<int>` over the allowed port range (1024..65535).
  - The function locks the mutex while sampling & probing to avoid concurrent port races inside the process.

- `is_free_port()` performs real bind operations using ephemeral sockets (TCP and UDP) to test availability. Because it actually performs bind() calls, it can be slow and has TOCTOU (time-of-check-to-time-of-use) implications.

## Functions (detailed)

### get_random_free_port()

Purpose

- Returns a `port` object representing a currently-free TCP+UDP port in the range 1024–65535.

Implementation summary

- Uses a thread-safe RNG (protected by `get_random_free_port_mutex`) to pick candidate ports.
- For each candidate port it calls `is_free_port()` to check both TCP and UDP bind availability.
- Repeats until a free port is found and returns it.

Important local variables

- RNG (`std::mt19937`) and distribution.
- A `port` wrapper for the sampled integer.

Edge cases and warnings

- The function may loop indefinitely if no free ports exist in the full search range (highly unlikely on normal systems).
- Port may be taken immediately after the function returns — this check is not a reservation.
- Because it does bind tests, privileged privileges or firewall rules can influence results.

Example

```cpp
auto p = hh_socket::get_random_free_port();
// Immediately bind to it to avoid races
```

### is_valid_port(port p)

Purpose

- Returns whether the `port` value represents a non-zero, explicit port usable for binding (1..65535).

Behavior

- Treats port `0` as invalid (0 has special meaning for auto-assignment in socket APIs).
- Returns `true` if `1 <= p.get() <= 65535`.

Example

```cpp
hh_socket::port p(8080);
bool ok = hh_socket::is_valid_port(p); // true
```

### is_free_port(port p)

Purpose

- Determines whether the provided port is currently bindable for both TCP and UDP (IPv4, INADDR_ANY).

Implementation summary

- Validates the port with `is_valid_port()`.
- Creates a TCP socket and attempts to bind to `INADDR_ANY` and the candidate port.
  - Uses `setsockopt(..., SO_REUSEADDR, ...)` to reduce false negatives due to TIME_WAIT semantics.
- Closes the TCP socket, then repeats using a UDP socket.
- Returns `true` only if both TCP and UDP bind attempts succeed.

Important local variables

- `tcp_socket`, `udp_socket` — raw socket handles used for probe binds
- `reuse` — integer set to enable `SO_REUSEADDR`
- `sockaddr_in addr` — zero-initialized structure with family AF_INET and port in network order

Platform details

- Uses IPv4 bind checks (`AF_INET`) only. An IPv6-only system or addresses bound to IPv6 may not be detected.
- Properly handles platform-specific `socket()` and `close()` calls.

Edge cases and warnings

- The check is inherently race-prone: another process may take the port immediately after the test.
- Using `SO_REUSEADDR` can change semantics on some platforms — it reduces false negatives but may produce false positives in corner cases.

Example

```cpp
hh_socket::port p(12345);
if (hh_socket::is_free_port(p)) {
    // port was free at test time
}
```

### convert_ip_address_to_network_order(const family &family_ip, const ip_address &address, void \*out_addr)

Purpose

- Converts a textual IP address into its binary network-order representation and writes it into `out_addr`.

Usage

- `out_addr` must point to a buffer large enough for the target family (4 bytes for IPv4 `in_addr`, 16 for IPv6 `in6_addr`).

Implementation details

- On Unix: uses `inet_pton()` with `family_ip.get()` and `address.get().c_str()`.
- On Windows: uses `InetPtonA()` where available.

Notes & warnings

- The function in the codebase does not always check the return value of the conversion helper; callers should verify and handle invalid addresses.
- Do not pass a null `out_addr`.

Example

```cpp
struct in_addr a;
hh_socket::convert_ip_address_to_network_order(hh_socket::family(hh_socket::IPV4), hh_socket::ip_address("127.0.0.1"), &a);
```

### get_ip_address_from_network_address(const sockaddr_storage &addr)

Purpose

- Extracts a textual IP address (string) from a `sockaddr_storage` containing either IPv4 or IPv6 data.

Implementation summary

- Uses `inet_ntop()` on the appropriate members (`sin_addr` or `sin6_addr`) and returns an std::string.

Edge cases

- If the family is not AF_INET or AF_INET6, or if `inet_ntop()` fails, the returned string may be empty. The implementation does not currently throw on failure.

Example

```cpp
sockaddr_storage ss = ...;
std::string ip = hh_socket::get_ip_address_from_network_address(ss);
```

### convert_host_to_network_order(int port) / convert_network_order_to_host(int port)

Purpose

- Small wrappers around `htons()` and `ntohs()` for port conversions. Return an `int` for convenience but operate on 16-bit port values.

Example

```cpp
int net_port = hh_socket::convert_host_to_network_order(8080);
int host_port = hh_socket::convert_network_order_to_host(net_port);
```

### initialize_socket_library() / cleanup_socket_library()

Purpose

- Platform-initialization helpers required on Windows to start/stop Winsock (WSAStartup/WSACleanup).

Behavior

- `initialize_socket_library()` returns `true` on success. On Unix it is a no-op and returns `true`.
- `cleanup_socket_library()` calls `WSACleanup()` on Windows and is a no-op on Unix.

Important notes

- Always call `initialize_socket_library()` on Windows before performing socket operations.

### close_socket(socket_t socket) / is_valid_socket(socket_t socket)

Purpose

- `close_socket()` closes a raw socket in a platform-appropriate manner (`closesocket()` on Windows, `close()` on Unix).
- `is_valid_socket()` returns whether the raw socket value is not equal to the platform invalid sentinel.

Example

```cpp
if (hh_socket::is_valid_socket(s)) hh_socket::close_socket(s);
```

### is_socket_open(int fd)

Purpose

- Attempts to determine if a numeric descriptor refers to an actual open socket.

Implementation summary

- Calls `getsockopt(..., SO_TYPE, ...)` and returns `true` if the call succeeds. This is a relatively robust check but not 100% definitive in all environments.

Edge cases

- `getsockopt` may fail for non-socket file descriptors or closed sockets. Permissions or platform limitations may affect the result.

## Protocol enum and address-family constants

### `enum class Protocol`

Purpose

- Provides a small, self-documenting mapping from high-level protocol names to the underlying socket type constants used when creating sockets.

Details

- `Protocol::TCP` maps to `SOCK_STREAM` and is used for connection-oriented, reliable streams (TCP).
- `Protocol::UDP` maps to `SOCK_DGRAM` and is used for connectionless datagram sockets (UDP).

Usage

- Pass `Protocol` to socket creation helpers to make the intent explicit instead of passing raw integers.

Example

```cpp
// create a TCP socket via lower-level API that expects SOCK_* constants
int sock_type = static_cast<int>(hh_socket::Protocol::TCP);
```

### `IPV4` and `IPV6`

Purpose

- Constants that map to the OS-level address-family identifiers (`AF_INET` / `AF_INET6`).

Notes

- Use `hh_socket::IPV4` and `hh_socket::IPV6` when calling functions that require an address family. They are simple aliases to improve readability and keep code consistent across the library.

## Additional utility functions (detailed)

### is_socket_connected(socket_t socket)

Purpose

- Determine whether a socket handle refers to a socket that is currently connected to a remote peer.

How it works (implementation notes)

- The implementation generally performs two checks:
  1. `getsockopt(..., SO_ERROR, ...)` is used to detect pending socket errors. A non-zero SO_ERROR indicates a connection problem.
  2. `getpeername()` is attempted; it returns 0 for connected sockets and -1 for sockets that are not connected (e.g., listening sockets or unconnected UDP sockets).
- The combination of these checks provides a robust indication that a socket is connected to a remote peer.

Platform differences and caveats

- Behavior is consistent on Unix and Windows, but API calls must be cast appropriately for each platform.
- For non-blocking connect(), SO_ERROR may contain delayed connection errors — check SO_ERROR before assuming a connection is established.
- The function returns `false` for listening sockets and for UDP sockets that are not explicitly connected with `connect()`.

Example

```cpp
if (hh_socket::is_socket_connected(sock)) {
    // safe to call send()/recv() for stream sockets
}
```

### get_error_message()

Purpose

- Provide a platform-aware, human-readable description of the last error encountered by socket or system calls.

How it works

- On Unix-like systems it typically calls `strerror_r(errno, buf, size)` or `strerror(errno)` to convert the global `errno` value into a string.
- On Windows it converts the result of `WSAGetLastError()` into a textual message using `FormatMessage()` and performs proper cleanup.

Notes

- The function returns a `std::string` and is safe to call from most contexts; however, exact thread-safety depends on which system calls are used internally (`strerror` is not thread-safe, `strerror_r` is preferred where available).
- Use `get_error_message()` right after a socket/system call that failed to capture the correct error value before it is overwritten by other operations.

Example

```cpp
if (!hh_socket::is_valid_socket(sock)) {
    std::cerr << "socket error: " << hh_socket::get_error_message() << '\n';
}
```

### to_upper_case(const std::string &input)

Purpose

- Return an uppercase copy of `input` without modifying the original string.

Implementation notes

- Implemented using `std::transform` and `std::toupper` (with a cast to unsigned char where needed) to avoid UB for negative `char` values.
- Does not depend on locale by default; use locale-aware conversions if required.

Example

```cpp
std::string s = hh_socket::to_upper_case("hello"); // "HELLO"
```

### make_listener_socket(uint16_t port, const std::string &ip = "0.0.0.0", int backlog = SOMAXCONN)

Purpose

- Convenience helper to create, bind and listen on a TCP non-blocking listening socket wrapped in the library's `socket` type.

Behavior and steps

- Creates a socket with `AF_INET` (by default) and `SOCK_STREAM`.
- Sets common socket options (for example `SO_REUSEADDR`) to make bind/rebind behavior more convenient.
- Sets the socket to non-blocking mode.
- Binds the socket to the provided `ip` and `port` (the `ip` defaults to all interfaces `0.0.0.0`).
- Calls `listen()` with the specified backlog.
- Wraps the resulting raw descriptor in a `std::shared_ptr<hh_socket::socket>` and returns it. On failure, the function returns `nullptr` (or throws, depending on the library's error policy).

Important notes

- `SO_REUSEADDR` semantics vary between platforms: on Unix it allows quick reuse of ports in TIME_WAIT, while on Windows additional flags (`SO_EXCLUSIVEADDRUSE`) may be desirable to avoid other processes binding the same port.
- This helper typically binds IPv4 addresses. For IPv6 or dual-stack support, an overload or additional parameters are required.

Example

```cpp
// create a non-blocking listener socket
auto listener = hh_socket::make_listener_socket(8080, "0.0.0.0", 128);
if (!listener) {
    std::cerr << "failed to create listener: " << hh_socket::get_error_message() << '\n';
}
```
