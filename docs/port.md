# port (Network port wrapper)

Source: `includes/port.hpp`

The `port` class is a small, type-safe wrapper for network port numbers. It enforces valid port values (0–65535) at construction time and provides convenient operations for comparisons and output. Using `port` helps prevent common mistakes when passing raw integers to networking APIs.

Key characteristics:

- Valid port range is 0 to 65535 (inclusive).
- Constructors are explicit to avoid accidental implicit conversions.
- Invalid values throw `hh_socket::socket_exception` (see `exceptions.hpp`).

## Class overview

Exposes:

- `explicit port()` — default uninitialized
- `explicit port(int id)` — validated constructor
- `int get() const` — retrieve the raw port number
- Comparison operators: `==`, `!=`, `<`
- `operator<<` stream output for logging/debugging

## Constructors

### `explicit port()`

Default constructor. Creates an uninitialized `port` object. The object must be assigned a valid port before use.

Example:

```cpp
hh_socket::port p; // uninitialized
```

### `explicit port(int id)`

Constructs a `port` and validates that `id` is within the allowed range (0–65535). If the value is out of range, the constructor throws `hh_socket::socket_exception` with a descriptive message.

Example:

```cpp
try {
    hh_socket::port p(8080); // valid
} catch (const hh_socket::socket_exception &e) {
    // handle invalid port
}
```

## Member functions

### `int get() const`

Returns the encapsulated port number as an `int`.

Example:

```cpp
hh_socket::port p(80);
int raw = p.get(); // 80
```

## Comparison operators

- `bool operator==(const port &other) const` — equality
- `bool operator!=(const port &other) const` — inequality
- `bool operator<(const port &other) const` — ordering

These operators make `port` suitable for use in associative containers and sorting algorithms.

Example:

```cpp
hh_socket::port a(22);
hh_socket::port b(80);
if (a < b) {
    // true
}
```

## Stream output

`operator<<` writes the numeric port value to an output stream, useful for logging and debugging.

Example:

```cpp
hh_socket::port p(443);
std::cout << "Port: " << p << '\n'; // prints: Port: 443
```

## Validation and exceptions

The private helper `set_port_id` performs validation and throws `hh_socket::socket_exception` if the provided port is outside the valid range. Callers that construct `port` objects from untrusted sources should catch this exception.

Example:

```cpp
try {
    hh_socket::port invalid(70000); // throws
} catch (const hh_socket::socket_exception &ex) {
    std::cerr << "Invalid port: " << ex.what() << '\n';
}
```

## Usage with other library types

`port` is designed to be used with `ip_address` and `family` when building `socket_address` objects and creating sockets with the library APIs.

Example (pseudo-code):

```cpp
hh_socket::ip_address ip("127.0.0.1");
hh_socket::family fam(hh_socket::IPV4);
hh_socket::port p(8080);
hh_socket::socket_address addr(ip, p, fam);
```

## Recommendations

- Prefer `port` over raw integers to make intent explicit and to catch invalid values early.
- Catch `socket_exception` when constructing from external input.
