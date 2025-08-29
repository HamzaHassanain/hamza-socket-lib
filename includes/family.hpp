#pragma once

#include <string>
#include <ostream>
#include <vector>
#include <algorithm>
#include "exceptions.hpp"
#include "utilities.hpp"

namespace hh_socket
{
    /**
     * @brief Represents an address family for socket operations (IPv4, IPv6).
     *
     * This class provides type-safe wrapper around socket address family constants,
     * preventing accidental misuse of raw integer values. It validates that only
     * supported address families (IPv4, IPv6) are used.
     *
     * @note Uses explicit constructors to prevent implicit conversions for type safety.
     *
     * Example usage:
     * @code
     * family ipv4_family(IPV4);
     * family ipv6_family(IPV6);
     * socket_address addr(ip_address("127.0.0.1"), port(8080), ipv4_family);
     * @endcode
     */
    class family
    {
    private:
        /// Allowed address family values (IPv4 and IPv6)
        std::vector<int> allowed_families = {IPV4, IPV6};

        /// Current address family ID
        int family_id;

        /**
         * @brief Validates and sets the family ID.
         * @param id The address family ID to set
         * @throws hh_socket::socket_exception if the ID is not IPV4 or IPV6
         */
        void set_family_id(int id)
        {
            if (std::find(allowed_families.begin(), allowed_families.end(), id) != allowed_families.end())
            {
                family_id = id;
            }
            else
            {
                throw socket_exception("Invalid family ID. Allowed families are IPV4 and IPV6.", "InvalidFamilyID", __func__);
            }
        }

    public:
        /**
         * @brief Default constructor - initializes to IPv4.
         *
         * Creates a family object with IPv4 as the default address family.
         * Marked explicit to prevent implicit conversions.
         */
        explicit family() : family_id(IPV4) {}

        /**
         * @brief Construct family with specific address family.
         * @param id Address family constant (IPV4 or IPV6)
         * @throws std::invalid_argument if id is not a valid address family
         *
         * Example:
         * @code
         * family ipv4(IPV4);  // Valid
         * family ipv6(IPV6);  // Valid
         * // family invalid(999);  // Throws exception
         * @endcode
         */
        explicit family(int id) { set_family_id(id); }

        // Use compiler-generated copy operations
        family(const family &other) = default;
        family &operator=(const family &other) = default;

        // Use compiler-generated move operations
        family(family &&other) = default;            //  for move operations
        family &operator=(family &&other) = default; //  for move assignment

        /**
         * @brief Get the raw address family value.
         * @return Integer value representing the address family (AF_INET, AF_INET6, etc.)
         */
        int get() const { return family_id; }

        /**
         * @brief Equality comparison operator.
         * @param other Family object to compare with
         * @return true if both objects have the same family ID
         */
        bool operator==(const family &other) const
        {
            return family_id == other.family_id;
        }

        /**
         * @brief Inequality comparison operator.
         * @param other Family object to compare with
         * @return true if objects have different family IDs
         */
        bool operator!=(const family &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Less-than comparison operator for ordering.
         * @param other Family object to compare with
         * @return true if this family's ID is less than other's ID
         */
        bool operator<(const family &other) const
        {
            return family_id < other.family_id;
        }

        /**
         * @brief Stream insertion operator for output.
         * @param os Output stream
         * @param f Family object to output
         * @return Reference to the output stream
         */
        friend std::ostream &operator<<(std::ostream &os, const family &f)
        {
            os << f.family_id;
            return os;
        }

        /// Default destructor
        ~family() = default;
    };
}