#ifndef SOCKET_HANDLER_H
#define SOCKET_HANDLER_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>

/**
 * @class SocketHandler
 * @brief Abstract base class for TCP and UDP socket handlers
 *
 * This class defines the interface for socket operations that are common
 * to both TCP and UDP protocols. It provides methods for initializing,
 * connecting, sending/receiving data, and closing sockets.
 */
class SocketHandler {
protected:
    /// File descriptor for the network socket
    int socket_fd_ = -1;
    /// Flag indicating whether to use IPv6 (true) or IPv4 (false)
    bool use_ipv6_ = false;

public:
    /**
     * @brief Virtual destructor
     */
    virtual ~SocketHandler() noexcept = default;
    
    /**
     * @brief Initialize the socket
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Start listening for connections on the specified port
     * @param port The port number to listen on
     * @return true if listening was successful, false otherwise
     */
    virtual bool listen(int port) = 0;
        
    /**
     * @brief Connect to a remote host
     * @param host The hostname or IP address to connect to
     * @param port The port number to connect to
     * @return true if connection was successful, false otherwise
     */
    virtual bool connect(const std::string& host, int port) = 0;
        
    /**
     * @brief Send data over the socket
     * @param data Pointer to the data to send
     * @param length Length of the data in bytes
     * @return Number of bytes sent, or -1 on error
     */
    virtual int send(const char* data, size_t length) = 0;
        
    /**
     * @brief Receive data from the socket
     * @param buffer Buffer to store the received data
     * @param buffer_size Size of the buffer in bytes
     * @return Number of bytes received, 0 on connection closure, or -1 on error
     */
    virtual int receive(char* buffer, size_t buffer_size) = 0;
        
    /**
     * @brief Close the socket connection
     */
    virtual void close() = 0;

    /**
     * @brief Get the socket file descriptor
     * @return The socket file descriptor
     */
    int getSocketFd() const;
    
    /**
     * @brief Set whether to use IPv6
     * @param use_ipv6 true to use IPv6, false to use IPv4
     */
    void setIPv6Mode(bool use_ipv6);
};

inline int SocketHandler::getSocketFd() const {
    return socket_fd_;
}

inline void SocketHandler::setIPv6Mode(bool use_ipv6) {
    use_ipv6_ = use_ipv6;
}

#endif // SOCKET_HANDLER_H
