#include "port_scanner.h"
#include "tcp_handler.h"
#include "udp_handler.h"
#include <cerrno>
#include <memory>

PortScanner::PortScanner(const std::string& host, bool use_tcp, bool use_ipv6) : host_(host), use_tcp_(use_tcp), use_ipv6_(use_ipv6) {}

PortScanner::~PortScanner() {}

bool PortScanner::scanPort(int port, int timeout_ms) {
    // Create a socket based on the protocol
    std::unique_ptr<SocketHandler> socket;
    if (use_tcp_) {
        auto tcp_socket = std::make_unique<TCPHandler>();
        tcp_socket->setIPv6Mode(use_ipv6_);
        socket = std::move(tcp_socket);
    } else {
        auto udp_socket = std::make_unique<UDPHandler>();
        udp_socket->setIPv6Mode(use_ipv6_);
        socket = std::move(udp_socket);
    }

    if (!socket->initialize()) {
        return false;
    }

    int socket_fd = socket->getSocketFd();
    // Fetch the file status for fd
    int flags = fcntl(socket_fd, F_GETFL, 0);
    // Set socket to non-blocking mode so we can handle timeouts manually.
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

    // Attempt to connect
    bool result = socket->connect(host_, port);

    if (use_tcp_) {
        // TCP-specific handling
        if (!result && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
            // Wait for connection using select()
            fd_set write_fds;
            struct timeval tv {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };

            FD_ZERO(&write_fds);
            FD_SET(socket_fd, &write_fds);

            int select_result = select(socket_fd + 1, nullptr, &write_fds, nullptr, &tv);
            
            if (select_result > 0) {
                // Check if connection was successful
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                result = (error == 0);
            } else {
                result = false;
            }
        }
    } else {
        // UDP-specific handling
        if (result) {
            // Send a packet and check for ICMP errors
            char dummy = 0;
            if (send(socket_fd, &dummy, 1, 0) < 0) {
                // If we immediately get ECONNREFUSED, it's closed
                if (errno == ECONNREFUSED) {
                    result = false;
                }
            }

            // Wait for a reply or an ICMP error
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(socket_fd, &read_fds);
            struct timeval tv {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };

            int sel = select(socket_fd + 1, &read_fds, nullptr, nullptr, &tv);
            
            if (sel > 0 && FD_ISSET(socket_fd, &read_fds)) {
                char buf[1];
                ssize_t n = recv(socket_fd, buf, sizeof(buf), 0);
                if (n >= 0) {
                    // Got some data back -> port is open
                    result = true;
                } else if (errno == ECONNREFUSED) {
                    // ICMP unreachable -> port closed
                    result = false;
                }
            } else {
                // Timeout (no response/ICMP) -> treat as closed
                result = false;
            }
        }
    }
    
    // Close the socket
    socket->close();
    return result;
}

std::vector<int> PortScanner::scanRange(int start_port, int end_port, int timeout_ms) {
    std::vector<int> open_ports;
    
    for (int port = start_port; port <= end_port; ++port) {
        if (scanPort(port, timeout_ms)) {
            open_ports.push_back(port);
        }
    }
    
    return open_ports;
}
