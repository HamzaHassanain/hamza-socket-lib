# socket_address (IP + port + family wrapper)

Source: `includes/socket_address.hpp` and `src/socket_address.cpp`

`socket_address` encapsulates a complete socket endpoint: an IP address (`ip_address`),
port (`port`), and address family (`family`). It constructs and owns the underlying
`sockaddr` structure used for system calls (bind, connect, accept, etc.).

Design goals

- Provide a type-safe, self-contained representation of an endpoint.
- Automatically create the correct `sockaddr_in` or `sockaddr_in6` depending on the family.
- Expose convenient accessors and raw pointers for use with OS socket APIs.

## Key behaviors and notes

- The class stores components as library wrapper types: `ip_address`, `port`, and `family`.
- Under the hood it holds a `std::shared_ptr<sockaddr>` called `addr` that points to a concrete
  `sockaddr_in` (IPv4) or `sockaddr_in6` (IPv6) instance. This shared pointer is returned by
  `get_sock_addr()` and used for system calls.
- Helper functions `handle_ipv4()` and `handle_ipv6()` initialize the appropriate `sockaddr_*`
  structures and assign them to the shared pointer.
- The constructors and copy operations create deep copies of the underlying `sockaddr` so that
  each `socket_address` instance owns its own storage.

## Constructors

### `socket_address()`

Default constructor. Creates an uninitialized object. `get_sock_addr()` may be `nullptr` until
initialized.

### `socket_address(const port &port_id, const ip_address &address = ip_address("0.0.0.0"), const family &family_id = family(IPV4))`

Constructs a `socket_address` from components. Internally calls `handle_ipv4()` and `handle_ipv6()`
which set `addr` to the appropriate `sockaddr` instance based on `family_id`.

Example:

```cpp
hh_socket::socket_address sa(hh_socket::port(8080), hh_socket::ip_address("127.0.0.1"), hh_socket::family(hh_socket::IPV4));
```

### `socket_address(sockaddr_storage &addr)`

Constructs from a system `sockaddr_storage` (for example, the value returned by `accept()`).

- Detects `addr.ss_family` and extracts IP, port and family.
- Copies the system structure into an internal `std::shared_ptr<sockaddr_in>` or `sockaddr_in6`.

Usage example (from accept loop):

```cpp
sockaddr_storage ss;
socklen_t len = sizeof(ss);
int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&ss), &len);
if (client_fd >= 0) {
    hh_socket::socket_address client_addr(ss);
    std::cout << "Client: " << client_addr.to_string() << '\n';
}
```

## Copy and move semantics

- Copy constructor and copy assignment create deep copies (allocate new `sockaddr_*`).
- Move operations are defaulted and transfer the wrappers efficiently.
- The `addr` member uses `std::shared_ptr`, but copying still results in new concrete `sockaddr` instances
  because helper functions are called in copy operations.

## Accessors

- `ip_address get_ip_address() const` — returns the `ip_address` component.
- `port get_port() const` — returns the `port` component.
- `family get_family() const` — returns the `family` component.
- `std::string to_string() const` — convenient "ip:port" representation.
- `sockaddr *get_sock_addr() const` — returns a raw pointer to the internal `sockaddr` suitable for system calls.
- `socklen_t get_sock_addr_len() const` — returns the length appropriate to the family (`sizeof(sockaddr_in)` or `sizeof(sockaddr_in6)`).

Important: `get_sock_addr()` returns the pointer owned by the `socket_address` instance; do not free it.

### `to_string()`

Purpose

- Return a concise "ip:port" textual representation of the socket address. This is a convenience helper that combines the `ip_address` and `port` components into a single human-readable string.

Behavior

- Produces the string `address.get() + ":" + std::to_string(port_id.get())`.
- Does not include family information (IPv4/IPv6) or brackets for IPv6; callers that need an RFC-compliant textual form for IPv6 should format the address with brackets (e.g., "[::1]:8080").

Example

```cpp
hh_socket::socket_address sa(hh_socket::port(8080), hh_socket::ip_address("127.0.0.1"), hh_socket::family(hh_socket::IPV4));
std::string s = sa.to_string(); // "127.0.0.1:8080"

// For IPv6, format with brackets if needed:
hh_socket::socket_address sa6(hh_socket::port(8080), hh_socket::ip_address("::1"), hh_socket::family(hh_socket::IPV6));
std::string s6 = "[" + sa6.get_ip_address().get() + "]:" + std::to_string(sa6.get_port().get()); // "[::1]:8080"
```

## Helper functions

### `handle_ipv4(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id)`

Creates and initializes a `sockaddr_in` using:

- `sin_family = family_id.get()`
- `sin_port = convert_host_to_network_order(port_id.get())`
- `sin_addr` populated by `convert_ip_address_to_network_order()`

### `handle_ipv6(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id)`

Creates and initializes a `sockaddr_in6` similarly for IPv6 fields.

These helpers are used internally by constructors and copy operations.

## Implementation details and caveats

- `convert_ip_address_to_network_order()` is used to translate textual IPs into the binary forms
  written into `sin_addr` / `sin6_addr`. If the textual IP is invalid, the resulting binary value
  may be undefined; callers should validate IP strings or handle conversion errors.
- `get_sock_addr_len()` returns `0` if the family is neither IPv4 nor IPv6; calling code should
  validate the return value before passing it to system calls.
- The class defaults to IPv4 (`IPV4`) when no family is provided.

## Example: binding a socket

```cpp
auto sa = hh_socket::socket_address(hh_socket::port(8080), hh_socket::ip_address("0.0.0.0"), hh_socket::family(hh_socket::IPV4));
int raw_sock = ::socket(AF_INET, SOCK_STREAM, 0);
::bind(raw_sock, sa.get_sock_addr(), sa.get_sock_addr_len());
::listen(raw_sock, 128);
```

Or using higher-level abstractions:

```cpp
hh_socket::socket_address sa(hh_socket::port(8080), hh_socket::ip_address("0.0.0.0"), hh_socket::family(hh_socket::IPV4));
hh_socket::socket s = hh_socket::socket(hh_socket::Protocol::TCP);
// hh_socket::socket s2 = hh_socket::socket(sa, hh_socket::Protocol::TCP); // s2 is automatically bound to the address
s.bind(sa);
s.listen(128);
```

## Debug output

- `operator<<` prints `IP Address: <ip>, Port: <port>, Family: <family>` to streams and uses the wrapper accessors.
