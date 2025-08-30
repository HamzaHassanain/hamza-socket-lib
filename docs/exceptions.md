# exceptions (socket_exception)

Source: `includes/exceptions.hpp`

This page documents the library's exception type `hh_socket::socket_exception`, the base exception for all socket-related errors in the project. It intentionally departs slightly from `std::exception` conventions to provide a typed error category and the name of the throwing function.

## Overview

`socket_exception` is intended as the canonical exception thrown by the library's socket operations. It contains:

- a human-readable message (provided at construction),
- a short type code (for programmatic handling), and
- the name of the function that raised the exception (useful for debugging and logging).

Because the class carries a type string, callers can inspect `e.type()` to react differently to different error classes (for example: "SocketCreation", "SocketBinding", "SocketOption").

## Public API

### Constructor

```cpp
explicit socket_exception(const std::string &message, const std::string &type, const std::string &thrower_function = "SOCKET_FUNCTION")
```

- `message`: descriptive text explaining the error condition.
- `type`: short identifier for the error category (e.g. "SocketCreation").
- `thrower_function`: optional name of the function that threw the exception; helpful for logs.

Example:

```cpp
throw socket_exception("Failed to bind", "SocketBinding", __func__);
```

### type()

```cpp
virtual std::string type() const noexcept;
```

Returns the exception type string supplied at construction. Use this for programmatic error handling:

```cpp
catch (const hh_socket::socket_exception &e) {
    if (e.type() == "SocketBinding") { /* recover or retry */ }
}
```

### thrower_function()

```cpp
virtual std::string thrower_function() const noexcept;
```

Returns the recorded function name where the exception was created.

### what()

```cpp
virtual std::string what();
```

Notes and important caveats:

- Unlike `std::exception::what()` which returns `const char *` and is `noexcept`, this class provides `what()` that returns a `std::string` (by value). That means it does not override `std::exception::what()` and will not be called polymorphically when catching `std::exception &`.
- The implementation composes and caches a formatted message that includes the type and thrower function. Because `what()` mutates an internal string member when called, concurrent calls to `what()` from multiple threads on the same exception object are not thread-safe. If thread-safety is required, capture the returned `std::string` into a local variable immediately after catching.

Example usage:

```cpp
try {
    // socket operation that may throw
} catch (hh_socket::socket_exception &e) {
    std::string msg = e.what(); // capture a thread-safe copy
    std::cerr << msg << std::endl;
}
```

## Best practices

- Catch `hh_socket::socket_exception &` (or specific derived exceptions if you add them later) to access `type()` and `thrower_function()`.
- Do not rely on `std::exception::what()` polymorphism for this class because it does not override the standard `what()` signature.
- When logging or rethrowing, call `e.what()` once and store the result rather than repeatedly calling it from multiple threads.

## Extending the hierarchy

`socket_exception` is designed to be a simple base. You can add derived exception classes that provide more semantic meaning (e.g. `socket_creation_exception`, `socket_binding_exception`) by inheriting from `socket_exception` and providing specialized constructors or additional context fields.

Example derived class pattern:

```cpp
class socket_binding_exception : public socket_exception {
public:
    socket_binding_exception(const std::string &message, const std::string &thrower = __func__)
        : socket_exception(message, "SocketBinding", thrower) {}
};
```

## Summary

`hh_socket::socket_exception` centralizes error information for the library: message, type, and thrower. When using the library, prefer catching this type to obtain structured error information; be mindful of the non-standard `what()` signature and thread-safety considerations when calling it.
