#ifndef TCP_HANDLER_H
#define TCP_HANDLER_H

#include "socket_handler.h"

/**
 * @class TCPHandler
 * @brief Handler for TCP socket operations
 *
 * This class implements the SocketHandler interface for TCP connections.
 * It provides methods for creating TCP servers and clients, handling
 * connections, and sending/receiving data over TCP sockets.
 */
class TCPHandler : public SocketHandler {
private:
    /// Socket addresses - including IP and port - used for connecting or binding sockets
    union {
        struct sockaddr_in address_v4;  ///< IPv4 address
        struct sockaddr_in6 address_v6; ///< IPv6 address
    };
    /// Flag indicating whether this is a server socket
    bool is_server_ = false;

public:
    /**
     * @brief Constructor
     */
    TCPHandler();
    
    /**
     * @brief Destructor
     */
    ~TCPHandler() noexcept override;

    /**
     * @brief Initialize the TCP socket
     * @return true if initialization was successful, false otherwise
     */
    bool initialize() override;
    
    /**
     * @brief Start listening for TCP connections on the specified port
     * @param port The port number to listen on
     * @return true if listening was successful, false otherwise
     */
    bool listen(int port) override;
    
    /**
     * @brief Accept an incoming TCP connection
     * @param client_handler TCPHandler object to handle the client connection
     * @return true if a connection was accepted, false otherwise
     */
    bool accept(TCPHandler& client_handler);
    
    /**
     * @brief Connect to a remote TCP server
     * @param host The hostname or IP address to connect to
     * @param port The port number to connect to
     * @return true if connection was successful, false otherwise
     */
    bool connect(const std::string& host, int port) override;
    
    /**
     * @brief Send data over the TCP connection
     * @param data Pointer to the data to send
     * @param length Length of the data in bytes
     * @return Number of bytes sent, or -1 on error
     */
    int send(const char* data, size_t length) override;
    
    /**
     * @brief Receive data from the TCP connection
     * @param buffer Buffer to store the received data
     * @param buffer_size Size of the buffer in bytes
     * @return Number of bytes received, 0 on connection closure, or -1 on error
     */
    int receive(char* buffer, size_t buffer_size) override;
    
    /**
     * @brief Close the TCP connection
     */
    void close() override;
};

#endif // TCP_HANDLER_H
