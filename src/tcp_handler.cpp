#include "tcp_handler.h"
#include <cstddef>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

TCPHandler::TCPHandler() {
    // Initialize addresses to 0
    memset(&address_v4, 0, sizeof(address_v4));
    memset(&address_v6, 0, sizeof(address_v6));
}

TCPHandler::~TCPHandler() noexcept {
    // Close the socket if it's still open
    close();
}

bool TCPHandler::initialize() {
    // Create a TCP socket
    socket_fd_ = socket(use_ipv6_ ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    /* 
        AF_INET : Use IPv4 internet protocols
        AF_INET6: Use IPv6 internet protocols
        SOCK_STREAM: Use TCP socket type
        0: Use the default protocol (TCP for SOCK_STREAM)
    */

    return socket_fd_ != -1;
}

bool TCPHandler::listen(int port) {
    // Allow reuse of address/port immediately after server is closed
    int opt = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (use_ipv6_) {
        // Setup the IPv6 address structure
        address_v6.sin6_family = AF_INET6;
        address_v6.sin6_addr = in6addr_any; // Accept connections on any IPv6 address
        address_v6.sin6_port = htons(port); // Convert port to network byte order

        // Bind socket to the address
        if (bind(socket_fd_, (struct sockaddr*)&address_v6, sizeof(address_v6)) < 0) {
            return false;
        }
    } else {
        // Setup the IPv4 address structure
        address_v4.sin_family = AF_INET; // IPv4
        address_v4.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address
        address_v4.sin_port = htons(port); // Convert port to network byte order

        // Bind socket to the address
        if (bind(socket_fd_, (struct sockaddr*)&address_v4, sizeof(address_v4)) < 0) {
            return false;
        }
    }

    // Start listening for connections
    if (::listen(socket_fd_, 5) < 0) { // 5 is backlog parameter
        return false;
    }

    is_server_ = true;
    return true;
}

bool TCPHandler::accept(TCPHandler& client_handler) {
    // Only server sockets can accept connections
    if (!is_server_) return false;
    
    // Accept a connection
    socklen_t addrlen; // Length of a socket address
    struct sockaddr* addr_ptr; // Pointer to address structure
    
    if (use_ipv6_) {
        addrlen = sizeof(address_v6);
        addr_ptr = (struct sockaddr*)&address_v6;
    } else {
        addrlen = sizeof(address_v4);
        addr_ptr = (struct sockaddr*)&address_v4;
    }
    
    client_handler.socket_fd_ = ::accept(socket_fd_, addr_ptr, &addrlen);
    client_handler.use_ipv6_ = use_ipv6_; // Inherit IPv6 mode from server

    return client_handler.socket_fd_ != -1;
}

bool TCPHandler::connect(const std::string& host, int port) {
    if (use_ipv6_) {
        // Setup the IPv6 address structure
        address_v6.sin6_family = AF_INET6;
        address_v6.sin6_port = htons(port);

        // If host is raw IPv6 string this succeeds, if not fall back to hostname resolution
        if (inet_pton(AF_INET6, host.c_str(), &address_v6.sin6_addr) <= 0) {
            // Resolve the hostname
            struct addrinfo hints, *result, *rp;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            
            std::string port_str = std::to_string(port);
            
            // Translate a hostname to a list of results for IPv6
            if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
                return false;
            }
            
            // Try each address until we successfully connect
            bool connected = false;
            for (rp = result; rp != NULL; rp = rp->ai_next) {
                if (rp->ai_family == AF_INET6) {
                    // Copy the resolved address
                    memcpy(&address_v6, rp->ai_addr, sizeof(struct sockaddr_in6));
                    
                    // Try to connect
                    if (::connect(socket_fd_, (struct sockaddr*)&address_v6, sizeof(address_v6)) != -1) {
                        connected = true;
                        break;
                    }
                }
            }
            
            // Cleanup
            freeaddrinfo(result);
            return connected;
        }

        // Connect to the server
        return ::connect(socket_fd_, (struct sockaddr*)&address_v6, sizeof(address_v6)) >= 0;
    } else {
        // Setup the IPv4 address structure
        address_v4.sin_family = AF_INET;
        address_v4.sin_port = htons(port);

        // If host is raw IPv4 string this succeeds, if not fall back to hostname resolution
        if (inet_pton(AF_INET, host.c_str(), &address_v4.sin_addr) <= 0) {
            // Resolve the hostname
            struct addrinfo hints, *result, *rp;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            
            std::string port_str = std::to_string(port);
            
            // Translate a hostname to a list of results for IPv6
            if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
                return false;
            }
            
            // Try each address until we successfully connect
            bool connected = false;
            for (rp = result; rp != NULL; rp = rp->ai_next) {
                if (rp->ai_family == AF_INET) {
                    // Copy the resolved address
                    memcpy(&address_v4, rp->ai_addr, sizeof(struct sockaddr_in));
                    
                    // Try to connect
                    if (::connect(socket_fd_, (struct sockaddr*)&address_v4, sizeof(address_v4)) != -1) {
                        connected = true;
                        break;
                    }
                }
            }
            
            freeaddrinfo(result);
            return connected;
        }

        // Connect to the server
        return ::connect(socket_fd_, (struct sockaddr*)&address_v4, sizeof(address_v4)) >= 0;
    }
}

int TCPHandler::send(const char* data, size_t length) {
    // Send data over the socket and return the number of bytes sent or -1 on error
    return ::send(socket_fd_, data, length, 0);
}

int TCPHandler::receive(char* buffer, size_t buffer_size) {
    // Receive data from the socket into provided buffer and 
    // return the number of bytes received, 0 on connection closure, or -1 on error
    return recv(socket_fd_, buffer, buffer_size, 0);
}

void TCPHandler::close() {
    if (socket_fd_ != -1) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}
