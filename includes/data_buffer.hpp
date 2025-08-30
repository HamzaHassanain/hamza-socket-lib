#pragma once

#include <string>
#include <vector>
#include "utilities.hpp"

namespace hh_socket
{
    /**
     * @brief A dynamic buffer for storing and managing binary data.
     *
     * This class provides a convenient wrapper around std::vector<char> for handling
     * binary data, strings, and character arrays. It offers efficient memory management
     * with automatic resizing and supports both string and raw data operations.
     *
     * The class is designed for scenarios where you need to accumulate data from
     * multiple sources (like network I/O, file reading, or HTTP parsing) and provides
     * seamless conversion between different data representations.
     *
     * @note Uses explicit constructors to prevent implicit conversions for type safety.
     */
    class data_buffer
    {
    private:
        /// Internal storage for the buffer data
        std::vector<char> buffer;

    public:
        /**
         * @brief Default constructor - creates an empty buffer.
         *
         * Creates a data_buffer with no initial data. The buffer will be empty
         * and ready to accept data through append operations.
         * Marked explicit to prevent implicit conversions.
         */
        explicit data_buffer() = default;

        /**
         * @brief Construct buffer from a string.
         * @param str String to initialize the buffer with
         *
         * Creates a data_buffer containing a copy of the string's characters.
         * The resulting buffer will have the same content as the string.
         */
        explicit data_buffer(const std::string &str) : buffer(str.begin(), str.end()) {}

        /**
         * @brief Construct buffer from raw character data.
         * @param data Pointer to the character data to copy
         * @param size Number of bytes to copy from data
         *
         * Creates a data_buffer containing a copy of the specified number of bytes
         * from the provided character array. This is useful for binary data that
         * may contain null bytes.
         *
         * @warning The caller must ensure that 'data' points to at least 'size' bytes
         *
         * Example:
         * @code
         * char raw_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x57};
         * data_buffer buf(raw_data, 7);  // Includes the null byte
         * @endcode
         */
        explicit data_buffer(const char *data, std::size_t size) : buffer(data, data + size) {}

        // Copy operations
        /**
         * @brief Copy constructor.
         * @param other Buffer to copy from
         *
         * Creates a deep copy of another data_buffer. The new buffer will contain
         * a complete copy of the source buffer's data.
         */
        data_buffer(const data_buffer &other) = default;

        /**
         * @brief Copy assignment operator.
         * @param other Buffer to copy from
         * @return Reference to this buffer after assignment
         *
         * Replaces this buffer's content with a copy of the other buffer's data.
         */
        data_buffer &operator=(const data_buffer &other) = default;

        // Move operations
        /**
         * @brief Move constructor.
         * @param other Buffer to move from
         *
         * Efficiently transfers ownership of the buffer data from another data_buffer.
         * The source buffer becomes empty after the move. This operation is O(1)
         * and doesn't copy the actual data.
         */
        data_buffer(data_buffer &&other) = default;

        /**
         * @brief Move assignment operator.
         * @param other Buffer to move from
         * @return Reference to this buffer after assignment
         */
        data_buffer &operator=(data_buffer &&other) = default;

        /**
         * @brief Append raw character data to the buffer.
         * @param data Pointer to the character data to append
         * @param size Number of bytes to append from data
         *
         * Adds the specified number of bytes from the character array to the end
         * of the buffer. The buffer will automatically resize to accommodate the
         * new data.
         *
         * @warning The caller must ensure that 'data' points to at least 'size' bytes
         */
        void append(const char *data, std::size_t size)
        {
            buffer.insert(buffer.end(), data, data + size);
        }

        /**
         * @brief Append a string to the buffer.
         * @param str String to append
         *
         * Adds all characters from the string to the end of the buffer.
         * This is equivalent to calling append(str.data(), str.size()).
         */
        void append(const std::string &str)
        {
            buffer.insert(buffer.end(), str.begin(), str.end());
        }

        /**
         * @brief Append another data_buffer to this buffer.
         * @param other Buffer to append
         *
         * Adds all bytes from the other buffer to the end of this buffer.
         */
        void append(const data_buffer &other)
        {
            buffer.insert(buffer.end(), other.buffer.begin(), other.buffer.end());
        }

        /**
         * @brief Get a pointer to the buffer's data.
         * @return Const pointer to the first byte of the buffer
         *
         * Returns a pointer to the internal character array. The pointer is valid
         * until the next non-const operation on the buffer. For empty buffers,
         * the returned pointer may or may not be null, but should not be dereferenced.
         */
        const char *data() const
        {
            return buffer.data();
        }

        /**
         * @brief Get the size of the buffer in bytes.
         * @return Number of bytes currently stored in the buffer
         *
         * Returns the total number of bytes contained in the buffer.
         * For empty buffers, this returns 0.
         */
        std::size_t size() const
        {
            return buffer.size();
        }

        /**
         * @brief Check if the buffer is empty.
         * @return true if the buffer contains no data, false otherwise
         *
         * This is equivalent to checking if size() == 0, but may be more efficient.
         */
        bool empty() const
        {
            return buffer.empty();
        }

        /**
         * @brief Clear all data from the buffer.
         *
         * Removes all data from the buffer, making it empty. size() will return 0 after this call.
         */
        void clear()
        {
            buffer.clear();
            buffer.shrink_to_fit();
        }

        /**
         * @brief Convert the buffer contents to a string.
         * @return String containing a copy of the buffer's data
         *
         * Creates a new std::string containing all the bytes from the buffer.
         * This operation creates a copy of the data, so the original buffer
         * remains unchanged.
         *
         * @note If the buffer contains null bytes, they will be included in the string
         */
        std::string to_string() const
        {
            return std::string(buffer.begin(), buffer.end());
        }

        /// Default destructor
        ~data_buffer() = default;
    };
}