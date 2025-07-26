#ifndef PORT_SCANNER_H
#define PORT_SCANNER_H

#include <string>
#include <vector>

/**
 * @class PortScanner
 * @brief Class for scanning ports on a remote host
 *
 * This class provides functionality for scanning individual ports or ranges of ports
 * on a remote host to determine if they are open. It supports both TCP and UDP
 * scanning, as well as IPv4 and IPv6 addresses.
 */
class PortScanner {
private:
    std::string host_; ///< Hostname or IP address to scan
    bool use_tcp_;     ///< Flag indicating whether to use TCP (true) or UDP (false)
    bool use_ipv6_;    ///< Flag indicating whether to use IPv6 (true) or IPv4 (false)

public:
    /**
     * @brief Constructor
     * @param host Hostname or IP address to scan
     * @param use_tcp Whether to use TCP (true) or UDP (false)
     * @param use_ipv6 Whether to use IPv6 (true) or IPv4 (false)
     */
    PortScanner(const std::string& host, bool use_tcp = true, bool use_ipv6 = false);
    
    /**
     * @brief Destructor
     */
    ~PortScanner();
        
    /**
     * @brief Scan a single port
     * @param port Port number to scan
     * @param timeout_ms Timeout in milliseconds for the scan
     * @return true if the port is open, false otherwise
     */
    bool scanPort(int port, int timeout_ms = 1000);
        
    /**
     * @brief Scan a range of ports
     * @param start_port First port in the range to scan
     * @param end_port Last port in the range to scan
     * @param timeout_ms Timeout in milliseconds for each port scan
     * @return Vector of open port numbers
     */
    std::vector<int> scanRange(int start_port, int end_port, int timeout_ms = 1000);
};

#endif
