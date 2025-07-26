#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "socket_handler.h"

/**
 * @class UDPHandler
 * @brief Handler for UDP socket operations
 *
 * This class implements the SocketHandler interface for UDP connections.
 * It provides methods for creating UDP servers and clients, and
 * sending/receiving datagrams over UDP sockets.
 */
class UDPHandler : public SocketHandler {
private:
    /// Socket address - including IP and port - used for connecting or binding sockets
    union {
        struct sockaddr_in address_v4;  ///< IPv4 address
        struct sockaddr_in6 address_v6; ///< IPv6 address
    };
    
    /// Socket address of the remote peer
    union {
        struct sockaddr_in peer_address_v4;  ///< IPv4 peer address
        struct sockaddr_in6 peer_address_v6; ///< IPv6 peer address
    };
    /// Flag indicating whether we have a peer address to send to
    bool is_connected_ = false;

public:
    /**
     * @brief Constructor
     */
    UDPHandler();
    
    /**
     * @brief Destructor
     */
    ~UDPHandler() noexcept override;

    /**
     * @brief Initialize the UDP socket
     * @return true if initialization was successful, false otherwise
     */
    bool initialize() override;
    
    /**
     * @brief Bind to a local port to receive UDP datagrams
     * @param port The port number to bind to
     * @return true if binding was successful, false otherwise
     */
    bool listen(int port) override;
    
    /**
     * @brief Set the remote peer address for sending UDP datagrams
     * @param host The hostname or IP address of the remote peer
     * @param port The port number of the remote peer
     * @return true if the peer address was set successfully, false otherwise
     */
    bool connect(const std::string& host, int port) override;
    
    /**
     * @brief Send a UDP datagram to the connected peer
     * @param data Pointer to the data to send
     * @param length Length of the data in bytes
     * @return Number of bytes sent, or -1 on error
     */
    int send(const char* data, size_t length) override;
    
    /**
     * @brief Receive a UDP datagram
     * @param buffer Buffer to store the received data
     * @param buffer_size Size of the buffer in bytes
     * @return Number of bytes received, or -1 on error
     */
    int receive(char* buffer, size_t buffer_size) override;
    
    /**
     * @brief Close the UDP socket
     */
    void close() override;
};

#endif
