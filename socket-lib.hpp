#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#ifndef SOCKET_PLATFORM_WINDOWS
#define SOCKET_PLATFORM_WINDOWS
#endif
#else
#ifndef SOCKET_PLATFORM_UNIX
#define SOCKET_PLATFORM_UNIX
#endif
#endif

#include "includes/connection.hpp"
#include "includes/data_buffer.hpp"
#include "includes/epoll_server.hpp"
#include "includes/exceptions.hpp"
#include "includes/family.hpp"
#include "includes/file_descriptor.hpp"
#include "includes/ip_address.hpp"
#include "includes/port.hpp"
#include "includes/socket_address.hpp"
#include "includes/socket.hpp"
#include "includes/tcp_server.hpp"
#include "includes/utilities.hpp"