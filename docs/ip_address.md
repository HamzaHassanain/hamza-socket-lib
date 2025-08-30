# ip_address (IP address wrapper)

Source: `includes/ip_address.hpp`

The `ip_address` class is a lightweight, type-safe wrapper for storing IP addresses as strings. It is designed for use with the library's networking types (e.g., `socket_address`, `socket`) and provides convenient access and comparison operators while avoiding implicit conversions.

Important characteristics:

- Stores IPv4 and IPv6 addresses as strings (e.g., "192.168.1.1" or "::1").
- Constructors are explicit to prevent accidental implicit conversions from raw strings.
- This class does not validate the format of the IP string — callers or higher-level APIs should validate addresses if required.

## Class overview

Exposes:

- `explicit ip_address()` — default, creates an empty address
- `explicit ip_address(const std::string &address)` — construct from string
- `const std::string &get() const` — access internal string
- Comparison operators: `==`, `!=`, `<`
- `operator<<` stream output for logging/debugging

## Constructors

### `explicit ip_address()`

Creates an empty `ip_address` object. Useful when the address will be assigned later.

Example:

```cpp
hh_socket::ip_address empty; // empty address
```

### `explicit ip_address(const std::string &address)`

Constructs an `ip_address` from a string. The constructor does not perform validation, so the provided string is stored as-is.

Example:

```cpp
hh_socket::ip_address ipv4("127.0.0.1");
hh_socket::ip_address ipv6("::1");
```

## Member functions

### `const std::string &get() const`

Returns a const reference to the underlying string representation of the address. Use this when you need to pass the raw address to lower-level APIs or for display.

Example:

```cpp
hh_socket::ip_address ip("192.168.0.10");
const std::string &s = ip.get();
std::cout << "IP: " << s << '\n';
```

## Comparison operators

- `bool operator==(const ip_address &other) const` — true if strings are equal
- `bool operator!=(const ip_address &other) const` — inverse of `==`
- `bool operator<(const ip_address &other) const` — lexicographic ordering on the stored string

These operators make `ip_address` usable in associative containers and sorting algorithms.

Example:

```cpp
hh_socket::ip_address a("10.0.0.1");
hh_socket::ip_address b("192.168.1.1");
if (a < b) {
    // lexical ordering
}
```

## Stream output

A `friend` `operator<<` writes the address string to an output stream. Useful for logging and debugging.

Example:

```cpp
hh_socket::ip_address ip("8.8.8.8");
std::cout << "Address: " << ip << '\n';
```

## Validation note

This class intentionally does not validate IP string format. If you need to ensure that an address is a valid IPv4 or IPv6 literal, validate the string before constructing `ip_address` or use higher-level APIs in the library that perform validation.

## Usage with other library types

`ip_address` is intended to be combined with `family` and `port` to build `socket_address` objects and to create sockets via the library's APIs.

Example (pseudo-code):

```cpp
hh_socket::ip_address ip("127.0.0.1");
hh_socket::family fam(hh_socket::IPV4);
hh_socket::port p(8080);
hh_socket::socket_address addr(ip, p, fam);
```
