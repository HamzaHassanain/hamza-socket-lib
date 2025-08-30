# data_buffer (Binary data buffer)

Source: `includes/data_buffer.hpp`

`data_buffer` is a small, efficient container for raw bytes. It wraps a `std::vector<char>` and provides explicit constructors, append helpers, and simple accessors intended for network I/O and protocol framing.

Key characteristics

- Stores raw bytes (not NUL-terminated strings); suitable for binary and textual protocols.
- Explicit constructors to avoid implicit conversions.
- Copyable (deep copy) and cheaply movable (O(1) move).
- Minimal API: append, inspect, clear, and convert-to-string.

## Class overview

Exposes:

- `explicit data_buffer()` — default empty buffer
- `explicit data_buffer(const std::string &str)` — initialize from string
- `explicit data_buffer(const char *data, std::size_t size)` — initialize from raw bytes
- `void append(const char *data, std::size_t size)` — append raw bytes
- `void append(const std::string &str)` — append string bytes
- `void append(const data_buffer &db)` — append another data_buffer
- `const char *data() const` — pointer to internal storage
- `std::size_t size() const` — current size in bytes
- `bool empty() const` — whether buffer is empty
- `void clear()` — clear contents
- `std::string to_string() const` — copy contents to std::string

## Constructors

### `explicit data_buffer()`

Create an empty buffer.

Example:

```cpp
hh_socket::data_buffer b; // empty
```

### `explicit data_buffer(const std::string &str)`

Create a buffer containing the bytes of `str` (null bytes preserved).

Example:

```cpp
hh_socket::data_buffer b(std::string("hello"));
```

### `explicit data_buffer(const char *data, std::size_t size)`

Create a buffer by copying `size` bytes from `data`. Caller must ensure `data` points to at least `size` bytes.

Example:

```cpp
char raw[] = {0x01, 0x02, 0x03};
hh_socket::data_buffer b(raw, sizeof(raw));
```

## Member functions

### `void append(const char *data, std::size_t size)`

Append `size` bytes from `data` to the buffer. Useful for accumulating network frames.

Example:

```cpp
buf.append(chunk.data(), chunk.size());
```

### `void append(const std::string &str)`

Append bytes of `str` to the buffer.

Example:

```cpp
buf.append(std::string(" world"));
```

### `const char* data() const`

Return pointer to internal contiguous storage. Do not dereference when `size() == 0`.

Usage:

```cpp
::send(fd, buf.data(), buf.size(), 0);
```

### `std::size_t size() const`

Return number of bytes stored.

### `bool empty() const`

Return true when `size() == 0`.

### `void clear()`

Remove all bytes from the buffer (capacity may remain). To release capacity swap with an empty buffer.

Example:

```cpp
hh_socket::data_buffer tmp;
swap(buf, tmp); // buf capacity released on many implementations
```

### `std::string to_string() const`

Return a `std::string` copy of the buffer contents. Embedded NUL bytes are preserved.

Example:

```cpp
std::string s = buf.to_string();
```

## Performance & notes

- Backed by `std::vector<char>`: contiguous storage suitable for OS I/O syscalls, amortized O(1) append.
- Move operations are O(1). Copying duplicates bytes.

## Examples

1. Build a request and send:

```cpp
hh_socket::data_buffer req;
req.append("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", 39);
conn.send(req);
```

2. Receive and convert to string:

```cpp
hh_socket::data_buffer r = conn.receive();
if (!r.empty()) {
    std::cout << r.to_string();
}
```

## Recommendations

- Use `data_buffer` for both binary frames and textual payloads.
- Catch exceptions at higher-level socket operations — `data_buffer` itself does not throw on append except for memory allocation failures.
- When reusing large buffers frequently, prefer clearing instead of reallocating to reduce allocations.
