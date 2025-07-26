#include "udp_handler.h"
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

UDPHandler::UDPHandler() {
    // Initialize addresses to 0
    memset(&address_v4, 0, sizeof(address_v4));
    memset(&address_v6, 0, sizeof(address_v6));
    memset(&peer_address_v4, 0, sizeof(peer_address_v4));
    memset(&peer_address_v6, 0, sizeof(peer_address_v6));
}

UDPHandler::~UDPHandler() noexcept {
    // Close the socket if it's still open
    close();
}

bool UDPHandler::initialize() {
    // Create a socket
    socket_fd_ = socket(use_ipv6_ ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
    /* 
        AF_INET : Use IPv4 internet protocols
        AF_INET6: Use IPv6 internet protocols
        SOCK_DGRAM: Use UDP socket type
        0: Use the default protocol (UDP for SOCK_DGRAM)
    */

    return socket_fd_ != -1;
}

bool UDPHandler::listen(int port) {
    if (use_ipv6_) {
        // Setup the IPv6 address structure
        address_v6.sin6_family = AF_INET6;
        address_v6.sin6_addr = in6addr_any;  // Accept connections on any IPv6 address
        address_v6.sin6_port = htons(port); // Convert port to network byte order

        // Bind the socket to the address
        return bind(socket_fd_, (struct sockaddr*)&address_v6, sizeof(address_v6)) >= 0;
    } else {
        // Setup the IPv4 address structure
        address_v4.sin_family = AF_INET;
        address_v4.sin_addr.s_addr = INADDR_ANY;  // Accept connections on any IP address
        address_v4.sin_port = htons(port); // Convert port to network byte order

        // Bind the socket to the address
        return bind(socket_fd_, (struct sockaddr*)&address_v4, sizeof(address_v4)) >= 0;
    }
}

bool UDPHandler::connect(const std::string& host, int port) {
    if (use_ipv6_) {
        peer_address_v6.sin6_family = AF_INET6;
        peer_address_v6.sin6_port = htons(port);

        // If host is raw IPv6 string this succeeds, if not fall back to hostname resolution
        if (inet_pton(AF_INET6, host.c_str(), &peer_address_v6.sin6_addr) <= 0) {
            // Resolve the hostname
            struct addrinfo hints, *result, *rp;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_DGRAM;
            
            std::string port_str = std::to_string(port);
            
            // Translate a hostname to a list of results for IPv6
            if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
                return false;
            }
            
            // Try each address until we find a suitable one
            bool found = false;
            for (rp = result; rp != NULL; rp = rp->ai_next) {
                if (rp->ai_family == AF_INET6) {
                    // Copy the resolved address
                    memcpy(&peer_address_v6, rp->ai_addr, sizeof(struct sockaddr_in6));
                    found = true;
                    break;
                }
            }
            
            // Cleanup
            freeaddrinfo(result);
            if (!found) {
                return false;
            }
        }
    } else {
        // Set up the IPv4 peer address structure
        peer_address_v4.sin_family = AF_INET;
        peer_address_v4.sin_port = htons(port);

        // If host is raw IPv4 string this succeeds, if not fall back to hostname resolution
        if (inet_pton(AF_INET, host.c_str(), &peer_address_v4.sin_addr) <= 0) {
            // Resolve the hostname
            struct addrinfo hints, *result, *rp;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            
            std::string port_str = std::to_string(port);
            
            // Translate a hostname to a list of results for IPv4
            if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
                return false;
            }
            
            // Try each address until we find a suitable one
            bool found = false;
            for (rp = result; rp != NULL; rp = rp->ai_next) {
                if (rp->ai_family == AF_INET) {
                    // Copy the resolved address
                    memcpy(&peer_address_v4, rp->ai_addr, sizeof(struct sockaddr_in));
                    found = true;
                    break;
                }
            }
            
            freeaddrinfo(result);
            if (!found) {
                return false;
            }
        }
    }

    // Mark as connected (we know the peer address)
    is_connected_ = true;
    return true;
}

int UDPHandler::send(const char* data, size_t length) {
    if (is_connected_) {
        // If we have a peer address, send to that address
        if (use_ipv6_) {
            return sendto(socket_fd_, data, length, 0, (struct sockaddr*)&peer_address_v6, sizeof(peer_address_v6));
        } else {
            return sendto(socket_fd_, data, length, 0, (struct sockaddr*)&peer_address_v4, sizeof(peer_address_v4));
        }
    } else {
        // If no peer address is set, we can't send
        return -1;
    }
}

int UDPHandler::receive(char* buffer, size_t buffer_size) {
    socklen_t addr_len;
    struct sockaddr* addr_ptr;
    
    if (use_ipv6_) {
        addr_len = sizeof(peer_address_v6);
        addr_ptr = (struct sockaddr*)&peer_address_v6;
    } else {
        addr_len = sizeof(peer_address_v4);
        addr_ptr = (struct sockaddr*)&peer_address_v4;
    }
    
    // Receive data and store the sender's address
    int bytes_received = recvfrom(socket_fd_, buffer, buffer_size, 0, addr_ptr, &addr_len);
    
    // If we received data successfully, mark as connected to the sender
    if (bytes_received > 0) {
        is_connected_ = true;
    }
    
    return bytes_received;
}

void UDPHandler::close() {
    if (socket_fd_ != -1) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}
