# family (Address Family wrapper)

Source: `includes/family.hpp`

The `family` class is a small, type-safe wrapper around address-family constants (for example IPv4 and IPv6). It prevents accidental misuse of raw integer values by validating allowed families and providing clear operations and conversions.

Key points:

- Only two address families are accepted: `IPV4` and `IPV6` (as defined in the project headers).
- Construction is explicit to avoid implicit conversions from integers.
- Invalid family IDs throw a `socket_exception`.

## Class overview

The class encapsulates a single integer representing the address family and exposes:

- explicit constructors (default to IPv4 or from an integer)
- comparison operators (`==`, `!=`, `<`)
- `get()` to retrieve the raw integer
- stream output operator for logging

This makes code that deals with address families clearer and safer than passing raw integers around.

## Constructors

### `explicit family()`

Default constructor. Initializes the object to IPv4.

Example:

```cpp
hh_socket::family f; // defaults to IPV4
```

### `explicit family(int id)`

Constructs a `family` with the provided integer id. The constructor validates the id and throws `hh_socket::socket_exception` if the id is not one of the allowed values (`IPV4` or `IPV6`).

Example:

```cpp
try {
    hh_socket::family f(hh_socket::IPV6); // valid
} catch (const hh_socket::socket_exception &e) {
    // handle invalid family id
}
```

## Member functions

### `int get() const`

Returns the underlying integer value of the address family. This is the raw value that can be passed to low-level APIs expecting `AF_INET`/`AF_INET6`-like constants.

Example:

```cpp
hh_socket::family f(hh_socket::IPV4);
int raw = f.get(); // use with OS socket APIs if needed
```

## Comparison operators

The class provides the usual comparison operations to make it easy to work with containers, sorting, and equality checks.

- `bool operator==(const family &other) const` — equality
- `bool operator!=(const family &other) const` — inequality
- `bool operator<(const family &other) const` — ordering

Example:

```cpp
hh_socket::family a(hh_socket::IPV4);
hh_socket::family b(hh_socket::IPV6);

if (a != b) {
    // different address families
}

std::vector<hh_socket::family> v = {b, a};
sort(v.begin(), v.end());
```

## Stream output

A `friend` stream operator is provided to write the numeric family id into an output stream. This is mainly useful for logging or debugging.

Example:

```cpp
hh_socket::family f(hh_socket::IPV4);
std::cout << "Family id: " << f << "\n"; // prints numeric id
```

## Exception behavior

The class validates family ids via a private `set_family_id` helper. If an invalid id is provided, it throws `hh_socket::socket_exception` with a descriptive message. Callers should catch this exception if the source of the id might be untrusted.

Example:

```cpp
try {
    hh_socket::family invalid(999); // throws socket_exception
} catch (const hh_socket::socket_exception &ex) {
    std::cerr << "Invalid family: " << ex.what() << '\n';
}
```

## Usage with other types in the library

The `family` type is intended to be used with other library types such as `socket_address` and `socket` so that APIs are type-safe and intention-revealing.

Example:

```cpp
// create an IPv4 socket address (pseudo-code, depends on library types)
hh_socket::family f(hh_socket::IPV4);
hh_socket::ip_address ip("127.0.0.1");
hh_socket::port p(8080);
hh_socket::socket_address addr(ip, p, f);
```

## Notes and recommendations

- Use the explicit `family` constructors to clearly document intent in the code.
- Prefer `family` over raw integers when working with the library's APIs to avoid subtle bugs.
- Catch `socket_exception` when constructing from external or user-provided integers.
