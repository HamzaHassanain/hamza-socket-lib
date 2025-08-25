#include "../includes/socket_address.hpp"
#include "../includes/utilities.hpp"

#include "../includes/ip_address.hpp"
#include "../includes/family.hpp"
#include "../includes/port.hpp"

namespace hh_socket
{
    // Constructs socket address from IP, port, and family components, creating appropriate sockaddr structure
    socket_address::socket_address(const port &port_id, const ip_address &address, const family &family_id)
        : address(address), family_id(family_id), port_id(port_id)
    {
        handle_ipv4(this, address, port_id, family_id);
        handle_ipv6(this, address, port_id, family_id);
    }

    // Constructs socket address from existing sockaddr_storage structure
    socket_address::socket_address(sockaddr_storage &addr)
    {
        // Check utility functions, to know about the internal implementations

        // If the IP family is IPv4
        if (addr.ss_family == IPV4)
        {
            auto ipv4_addr = reinterpret_cast<sockaddr_in *>(&addr);

            address = ip_address(get_ip_address_from_network_address(addr));
            port_id = port(convert_network_order_to_host(ipv4_addr->sin_port));
            family_id = family(IPV4);

            // Converts shared_ptr<sockaddr_in> to shared_ptr<sockaddr>
            // This is a safe cast because sockaddr_in can be safely treated as sockaddr
            this->addr = std::reinterpret_pointer_cast<sockaddr>(std::make_shared<sockaddr_in>(*ipv4_addr));
        }
        else if (addr.ss_family == IPV6)
        {
            auto ipv6_addr = reinterpret_cast<sockaddr_in6 *>(&addr);
            address = ip_address(get_ip_address_from_network_address(addr));
            port_id = port(convert_network_order_to_host(ipv6_addr->sin6_port));
            family_id = family(IPV6);

            // Converts shared_ptr<sockaddr_in6> to shared_ptr<sockaddr>
            // This is a safe cast because sockaddr_in6 can be safely treated as sockaddr
            this->addr = std::reinterpret_pointer_cast<sockaddr>(std::make_shared<sockaddr_in6>(*ipv6_addr));
        }
    }
    // Copy constructor that duplicates socket address and recreates sockaddr structure
    socket_address::socket_address(const socket_address &other)
        : address(other.address), family_id(other.family_id), port_id(other.port_id)
    {
        if (other.addr)
        {
            handle_ipv4(this, other.address, other.port_id, other.family_id);
            handle_ipv6(this, other.address, other.port_id, other.family_id);
        }
    }

    // Copy assignment operator that assigns components and recreates sockaddr structure
    socket_address &socket_address::operator=(const socket_address &other)
    {
        if (this != &other)
        {
            address = other.address;
            family_id = other.family_id;
            port_id = other.port_id;

            if (other.addr)
            {
                handle_ipv4(this, other.address, other.port_id, other.family_id);
                handle_ipv6(this, other.address, other.port_id, other.family_id);
            }
        }
        return *this;
    }

    // Returns the size of the sockaddr structure based on address family
    socklen_t socket_address::get_sock_addr_len() const
    {
        if (family_id.get() == IPV4)
        {
            // Return size for IPv4 sockaddr_in
            return sizeof(sockaddr_in);
        }
        else if (family_id.get() == IPV6)
        {
            // Return size for IPv6 sockaddr_in6
            return sizeof(sockaddr_in6);
        }
        return 0;
    }

    // Returns raw pointer to sockaddr structure for system calls
    sockaddr *socket_address::get_sock_addr() const
    {
        return addr.get();
    }

    // Helper function that creates and initializes IPv4 sockaddr_in structure
    void handle_ipv4(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id)
    {
        auto cur_addr = std::make_shared<sockaddr_in>();
        cur_addr->sin_family = family_id.get();
        cur_addr->sin_port = convert_host_to_network_order(port_id.get());
        convert_ip_address_to_network_order(family_id, address, &cur_addr->sin_addr);

        addr->addr = std::reinterpret_pointer_cast<sockaddr>(cur_addr);
    }

    // Helper function that creates and initializes IPv6 sockaddr_in6 structure
    void handle_ipv6(socket_address *addr, const ip_address &address, const port &port_id, const family &family_id)
    {
        auto cur_addr = std::make_shared<sockaddr_in6>();
        cur_addr->sin6_family = family_id.get();
        cur_addr->sin6_port = convert_host_to_network_order(port_id.get());
        convert_ip_address_to_network_order(family_id, address, &cur_addr->sin6_addr);

        addr->addr = std::reinterpret_pointer_cast<sockaddr>(cur_addr);
    }
}
