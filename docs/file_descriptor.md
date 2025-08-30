# file_descriptor (Cross-platform socket/file descriptor wrapper)

Source: `includes/file_descriptor.hpp`

`file_descriptor` is a small, move-only RAII-style wrapper around platform-specific
socket handles and file descriptors. It provides a unified interface for both
Unix-like `int` file descriptors and Windows `SOCKET` handles, makes ownership
explicit, and prevents accidental copying which could lead to double-closes.

Key characteristics

- Move-only semantics (copy operations deleted) to ensure single ownership.
- Works with platform type aliases: `socket_t` is `int` on Unix and `SOCKET` on Windows.
- Provides utility methods for validity checks and safe transfer of ownership.

## Platform notes

- On Unix-like systems the underlying type is `int` and invalid descriptors use `-1`.
- On Windows the underlying type is `SOCKET` (unsigned) and invalid value is `INVALID_SOCKET`.
- `file_descriptor` abstracts these differences and exposes a consistent API.

## Constructors

### `file_descriptor()`

Default constructor. Creates an invalid descriptor (`INVALID_SOCKET_VALUE`).
Useful when you need a placeholder that will later be assigned a valid descriptor.

Example:

```cpp
hh_socket::file_descriptor fd; // invalid until assigned
```

### `explicit file_descriptor(socket_t fd)`

Wraps an existing raw socket handle or file descriptor. The wrapper does not
duplicate the handle; it takes ownership semantics (the caller should not
close the raw descriptor after moving it into `file_descriptor`).

Example:

```cpp
// pseudo-code: create raw socket via platform API
socket_t raw = ::socket(...);
hh_socket::file_descriptor fd(raw);
```

## Move semantics

- `file_descriptor(file_descriptor &&)` — move constructor transfers ownership and
  leaves the source invalid.
- `file_descriptor &operator=(file_descriptor &&)` — move assignment transfers ownership
  with self-assignment protection.

These ensure a clear single-owner lifetime for the underlying OS resource.

Example:

```cpp
hh_socket::file_descriptor a(raw_fd);
hh_socket::file_descriptor b(std::move(a)); // a now invalid
```

## Methods

### `int get() const`

Returns the underlying raw descriptor value. On invalid objects this returns
`INVALID_SOCKET_VALUE`. Use with care in cross-platform code because the
numeric type differs between platforms.

Example:

```cpp
int raw = fd.get();
```

### `bool is_valid() const`

Checks whether the wrapper currently holds a valid descriptor.

Example:

```cpp
if (fd.is_valid()) {
    // safe to use
}
```

### `void invalidate()`

Invalidates the wrapper without closing the underlying resource (sets to
`INVALID_SOCKET_VALUE`). Useful when ownership has been transferred elsewhere
or the resource was closed by external code.

Example:

```cpp
fd.invalidate(); // fd becomes invalid
```

## Comparison and ordering

- `operator==`, `operator!=` compare the wrapped raw values.
- `operator<` provides ordering so instances can be used in ordered containers
  like `std::set` or `std::map`.

Example:

```cpp
if (fd1 == fd2) { }
std::set<hh_socket::file_descriptor> s; // requires valid ordering semantics
```

## Stream output

`operator<<` writes either the numeric descriptor value or the string
"INVALID_FILE_DESCRIPTOR" for invalid wrappers. Useful for debugging and logs.

Example:

```cpp
std::cout << fd << '\n';
```

## Safety and recommendations

- The class is move-only to prevent accidental resource duplication. Always
  `std::move` when transferring ownership.
- Do not copy `file_descriptor`; the copy constructors/operators are deleted.
- Be careful when calling `get()` in cross-platform code — the numeric
  representation may differ between platforms.
- If you need the wrapper to close the underlying descriptor on destruction,
  ensure the surrounding code follows a single ownership policy (this wrapper
  itself does not explicitly close the raw descriptor in the header).

## Example usage (typical)

```cpp
// create socket (platform API), then wrap it
socket_t raw = ::socket(AF_INET, SOCK_STREAM, 0);
if (raw == INVALID_SOCKET_VALUE) {
    // handle error
}

hh_socket::file_descriptor fd(raw);
if (fd.is_valid()) {
    std::cout << "descriptor: " << fd << '\n';
}

// transfer ownership
hh_socket::file_descriptor other = std::move(fd);
// fd is now invalid
```
