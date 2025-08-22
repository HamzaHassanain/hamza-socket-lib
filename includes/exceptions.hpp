#pragma once

#include <stdexcept>
#include <string>

namespace hamza_socket
{
    /**
     * @brief Base exception class for all socket-related errors.
     *
     * This class serves as the base for all socket operation exceptions in the library.
     * It extends std::runtime_error to provide a consistent exception hierarchy for
     * socket programming errors. All derived exceptions inherit from this class,
     * allowing for both specific and general exception handling.
     *
     * The class provides a virtual type() method that can be overridden by derived
     * classes to provide specific exception type identification.
     *
     * Example usage:
     * @code
     * try {
     *     // Socket operations that might fail
     *     perform_socket_operation();
     * }
     * catch (const socket_exception& e) {
     *     std::cerr << e.what() << std::endl;
     * }
     * @endcode
     */
    class socket_exception : public std::runtime_error
    {
        std::string _type;
        std::string _thrower_function;
        mutable std::string _formatted_message; // Cache for formatted message

    public:
        /**
         * @brief Construct exception with error message.
         * @param message Descriptive error message explaining the socket failure
         * @param type Type of the socket exception
         * @param thrower_function Name of the function that threw the exception
         */
        explicit socket_exception(const std::string &message, const std::string &type, const std::string &thrower_function = "SOCKET_FUNCTION")
            : std::runtime_error(message), _type(type), _thrower_function(thrower_function) {}

        /**
         * @brief Get the exception type name.
         * @return C-style string identifying the exception type
         */
        virtual const char *type() const noexcept
        {
            return _type.c_str();
        }

        /**
         * @brief Get the name of the function that threw the exception.
         * @return C-style string identifying the thrower function
         */
        virtual const char *thrower_function() const noexcept
        {
            return _thrower_function.c_str();
        }

        /**
         * @brief Get the formatted error message string.
         * @return C-style string containing the formatted error message
         * @note Thread-safe and returns a persistent pointer to the formatted message
         */
        virtual const char *what() const noexcept override
        {
            // Create formatted message once and cache it
            if (_formatted_message.empty())
            {
                _formatted_message = "Socket Exception [" + _type + "] in " + _thrower_function + ": " + std::runtime_error::what();
            }
            return _formatted_message.c_str();
        }

        /// Default virtual destructor
        virtual ~socket_exception() = default;
    };
}