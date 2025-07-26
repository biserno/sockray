/**
 * @file main.cpp
 * @brief Main implementation file for the sockray netcat clone
 * 
 * This file contains the main function and helper functions for the sockray
 * application, a netcat clone that supports TCP and UDP connections, port
 * scanning, and process execution with I/O redirection.
 */

#include "socket_handler.h"
#include "tcp_handler.h"
#include "udp_handler.h"
#include "port_scanner.h"
#include "process_server.h"
#include "utils.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

/// Global flag for handling Ctrl + C
volatile sig_atomic_t running = 1;

/**
 * @brief Signal handler for Ctrl + C
 * @param signal Signal number
 */
void signalHandler(int signal) {
    if (signal == SIGINT) {
        running = 0;
        // Force exit if program doesn't terminate within 2 seconds
        alarm(2);
    }
}

/**
 * @brief Alarm handler for forced exit
 * @param signal Signal number
 */
void alarmHandler(int signal) {
    std::cerr << "\nForced exit due to timeout\n";
    exit(1);
}

/**
 * @brief Check if a string is a number
 * @param s String to check
 * @return true if the string is a number, false otherwise
 */
bool isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

/**
 * @brief Print usage information
 * @param programName Name of the program
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] [hostname] [port]\n\n"
              << "Options:\n"
              << "  -6, --ipv6                Use IPv6 addresses only.\n"
              << "  -l, --listen              Listen mode (server)\n"
              << "  -p, --port PORT           Specify local port for listen mode\n"
              << "  -u, --udp                 Use UDP instead of TCP\n"
              << "  -s, --scan PORT           Scan a single port\n"
              << "  -r, --range START-END     Scan a range of ports\n"
              << "  -e, --execute COMMAND     Execute command and redirect I/O to network\n"
              << "  -v, --verbose             Verbose mode\n"
              << "  -h, --help                Display this help message\n\n"
              << "Examples:\n"
              << "  " << programName << " localhost 80                  # Connect to localhost port 80\n"
              << "  " << programName << " --listen --port 8080          # Listen on port 8080\n"
              << "  " << programName << " --udp localhost 53            # UDP connection to localhost port 53\n"
              << "  " << programName << " --scan 80 localhost           # Check if port 80 is open\n"
              << "  " << programName << " --range 1-1000 localhost      # Scan ports 1-1000\n"
              << "  " << programName << " --listen --port 8080 --execute \"/bin/bash\"  # Set up a remote shell\n"
              << "  " << programName << " --listen 8080 --execute \"/bin/bash\"         # Same as above\n";
}

/**
 * @brief Run a TCP server
 * 
 * This function creates a TCP server that listens on the specified port.
 * If a command is provided, it will execute the command and redirect its
 * I/O to the client connection. Otherwise, it will act as an echo server.
 * 
 * @param port Port number to listen on
 * @param verbose Whether to print verbose output
 * @param use_ipv6 Whether to use IPv6
 * @param command Command to execute (optional)
 */
void runTCPServer(int port, bool verbose, bool use_ipv6, const std::string& command = "") {
    TCPHandler server;
    server.setIPv6Mode(use_ipv6);
    
    if (!server.initialize()) {
        std::cerr << "Failed to initialize server socket" << std::endl;
        return;
    }
    
    if (!server.listen(port)) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return;
    }
    
    verboseLog(verbose, "TCP Server listening on port " + std::to_string(port) + "...");
    
    TCPHandler client;
    if (!server.accept(client)) {
        std::cerr << "Failed to accept client connection" << std::endl;
        return;
    }
    
    verboseLog(verbose, "Client connected");
    
    if (!command.empty()) {
        ProcessServer process_server;
        verboseLog(verbose, "Starting process: " + command);
        
        if (!process_server.startProcess(command, client)) {
            std::cerr << "Failed to start process" << std::endl;
            return;
        }
        
        // Create a thread to check if client is still connected
        std::thread client_check_thread([&client, verbose]() {
            while (running) {
                // Set socket to non-blocking mode temporarily
                int flags = fcntl(client.getSocketFd(), F_GETFL, 0);
                fcntl(client.getSocketFd(), F_SETFL, flags | O_NONBLOCK);
                
                // Try to receive data without actually removing it from the queue
                char test_buffer[1];
                int test_result = recv(client.getSocketFd(), test_buffer, sizeof(test_buffer), MSG_PEEK);
                
                // Restore blocking mode
                fcntl(client.getSocketFd(), F_SETFL, flags);
                
                if (test_result == 0) {
                    verboseLog(verbose, "Client disconnected, terminating process");
                    running = false;
                    std::exit(0);
                    break;
                } else if (test_result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "Error checking client connection: " << strerror(errno) << std::endl;
                    running = false;
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
        
        // Wait for the process to finish or for Ctrl + C
        while (process_server.isRunning() && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Clean up the client check thread
        if (client_check_thread.joinable()) {
            client_check_thread.join();
        }
        
        process_server.stopProcess();
        verboseLog(verbose, "Process terminated");
    } else {
        const size_t buffer_size = 4096;
        char buffer[buffer_size];
        // Create a thread to receive data from the client
        std::thread receive_thread([&client, verbose]() {
            const size_t buffer_size = 4096;
            static int counter = 0;
            char buffer[buffer_size];

            while (running) {
                // Clear the buffer
                memset(buffer, 0, sizeof(buffer));


                int bytes_received = client.receive(buffer, sizeof(buffer));
                if (bytes_received > 0) {
                    counter += 1;
                    if (verbose) {
                    std::cout << "Client: " << buffer << counter << std::endl;
                } else {
                    std::cout << buffer << counter;
                    std::cout.flush();
                    }
                } else if (bytes_received == 0) {
                    verboseLog(verbose, "Client disconnected");
                    running = false;
                    exit(0);
                    break;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });

        // Detach the thread so it runs independently
        receive_thread.detach();

        // Send data from stdin to the client
        std::string line;
        char input_buffer[4096];

        while (running) {
            // Read from stdin
            if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
                break;
            }

            // Send the data
            int bytes_sent = client.send(input_buffer, strlen(input_buffer));
            
            if (bytes_sent < 0) {
                std::cerr << "Failed to send data" << std::endl;
                break;
            }
        }

        // Wait for the receive thread to finish
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief Run a UDP server
 * 
 * This function creates a UDP server that listens on the specified port.
 * It receives datagrams from clients and displays them. If the server
 * tries to send data to a connected client, it will disconnect.
 * 
 * @param port Port number to listen on
 * @param verbose Whether to print verbose output
 * @param use_ipv6 Whether to use IPv6
 * @param command Command to execute (not supported for UDP, should be empty)
 */
void runUDPServer(int port, bool verbose, bool use_ipv6, const std::string& command = "") {
    UDPHandler server;
    server.setIPv6Mode(use_ipv6);
    
    if (!server.initialize()) {
        std::cerr << "Failed to initialize server socket" << std::endl;
        return;
    }

    if (!server.listen(port)) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return;
    }

    verboseLog(verbose, "UDP Server listening on port " + std::to_string(port) + "...");

    bool client_connected = false;
    auto last_received = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::minutes(5);

    std::thread receive_thread([&]() {
        const size_t buffer_size = 4096;
        char buffer[buffer_size];

        while (running) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = server.receive(buffer, sizeof(buffer));

            if (bytes_received > 0) {
                client_connected = true;
                if (verbose) {
                    std::cout << "Client: " << buffer << std::endl;
                } else {
                    std::cout << buffer;
                    std::cout.flush();
                }
                last_received = std::chrono::steady_clock::now();
            } else if (bytes_received < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_received) > timeout) {
                    verboseLog(verbose, "Client timeout, no data received for " + std::to_string(timeout.count()) + " minutes");
                    running = false;
                    exit(0);
                }
            }
        }
    });

    // Detach the thread so it runs independently
    receive_thread.detach();

    char input_buffer[4096];
    while (running) {
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            break;
        }

        if (client_connected) {
            verboseLog(verbose, "Server attempted to send data to a connected client, disconnecting");
            running = false;
            exit(0);
        } else {
            verboseLog(verbose, "No client connected. Message sent to void.");
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * @brief Run a TCP client
 * 
 * This function creates a TCP client that connects to the specified host and port.
 * It sends data from stdin to the server and displays data received from the server.
 * 
 * @param host Hostname or IP address to connect to
 * @param port Port number to connect to
 * @param verbose Whether to print verbose output
 * @param use_ipv6 Whether to use IPv6
 */
void runTCPClient(const std::string& host, int port, bool verbose, bool use_ipv6) {
    TCPHandler client;
    client.setIPv6Mode(use_ipv6);
    
    if (!client.initialize()) {
        std::cerr << "Failed to initialize client socket" << std::endl;
        return;
    }
    
    verboseLog(verbose, "Connecting to " + host + ":" + std::to_string(port) + "...");

    if (!client.connect(host, port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }
    
    verboseLog(verbose, "Connected to server");
    
    // Create a thread to receive data from the server
    std::thread receive_thread([&client, verbose]() {
        const size_t buffer_size = 4096;
        char buffer[buffer_size];
        
        while (running) {
            // Clear the buffer
            memset(buffer, 0, sizeof(buffer));
            
            int bytes_received = client.receive(buffer, sizeof(buffer));
            
            if (bytes_received > 0) {
                if (verbose) {
                    std::cout << "Server: " << buffer << std::endl;
                } else {
                    std::cout << buffer;
                    std::cout.flush();
                }
            } else if (bytes_received == 0) {
                verboseLog(verbose, "Server disconnected");
                running = false;
                // Exit the program when server disconnects
                exit(0);
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
    
    // Detach the thread so it runs independently
    receive_thread.detach();
    
    // Send data from stdin to the server
    std::string line;
    char buffer[4096];
    
    while (running) {
        // Read from stdin
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            break;
        }
        
        // Send the data
        int bytes_sent = client.send(buffer, strlen(buffer));
        
        if (bytes_sent < 0) {
            std::cerr << "Failed to send data" << std::endl;
            break;
        }
    }
    
    // Wait for the receive thread to finish
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

/**
 * @brief Run a UDP client
 * 
 * This function creates a UDP client that connects to the specified host and port.
 * It sends data from stdin to the server and displays data received from the server.
 * If the server is not listening, it will disconnect after sending the first message.
 * 
 * @param host Hostname or IP address to connect to
 * @param port Port number to connect to
 * @param verbose Whether to print verbose output
 * @param use_ipv6 Whether to use IPv6
 */
void runUDPClient(const std::string& host, int port, bool verbose, bool use_ipv6) {
    UDPHandler client;
    client.setIPv6Mode(use_ipv6);
    
    if (!client.initialize()) {
        std::cerr << "Failed to initialize client socket" << std::endl;
        return;
    }

    if (!client.connect(host, port)) {
        std::cerr << "Failed to set server address" << std::endl;
        return;
    }

    verboseLog(verbose, "UDP Client connected to " + host + ":" + std::to_string(port));

    bool confirmed_listener = false;
    bool first_message_sent = false;

    std::thread receive_thread([&]() {
        const size_t buffer_size = 4096;
        char buffer[buffer_size];
        auto last_received = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(3);

        while (running) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = client.receive(buffer, sizeof(buffer));
            if (bytes_received > 0) {
                confirmed_listener = true;
                std::cout << buffer;
                std::cout.flush();
                last_received = std::chrono::steady_clock::now();
            } else if (bytes_received < 0 && first_message_sent) {
                auto now = std::chrono::steady_clock::now();
                if (!confirmed_listener && std::chrono::duration_cast<std::chrono::seconds>(now - last_received) > timeout) {
                    verboseLog(verbose, "No server listening. Exiting.");
                    running = false;
                    exit(0);
                }
            }
        }
    });

    // Detach the thread so it runs independently
    receive_thread.detach();

    char buffer[4096];
    while (running) {
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        int bytes_sent = client.send(buffer, strlen(buffer));
        first_message_sent = true;

        if (bytes_sent < 0) {
            if (!confirmed_listener) {
                std::cerr << "Send failed. Server may not be listening. Disconnecting client." << std::endl;
                break;
            } else {
                std::cerr << "Send error: " << strerror(errno) << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    running = false;
    std::exit(0);
}

/**
 * @brief Scan ports on a remote host
 * 
 * This function scans a single port or a range of ports on a remote host
 * to determine if they are open. It supports both TCP and UDP scanning.
 * 
 * @param host Hostname or IP address to scan
 * @param port Port number to scan, or start of range
 * @param end_port End of port range (0 for single port scan)
 * @param use_tcp Whether to use TCP (true) or UDP (false)
 * @param verbose Whether to print verbose output
 * @param use_ipv6 Whether to use IPv6
 */
void scanPorts(const std::string& host, int port, int end_port, bool use_tcp, bool verbose, bool use_ipv6) {
    PortScanner scanner(host, use_tcp, use_ipv6);
    
    if (end_port > 0) {
        // Scan a range of ports
        verboseLog(verbose, "Scanning " + std::string(use_tcp ? "TCP" : "UDP") + " ports " 
                  + std::to_string(port) + "-" + std::to_string(end_port) + " on " + host + "...");
        
        auto open_ports = scanner.scanRange(port, end_port);
        
        std::cout << "Open ports: ";
        if (open_ports.empty()) {
            std::cout << "None";
        } else {
            for (int p : open_ports) {
                std::cout << p << " ";
            }
        }
        std::cout << std::endl;
    } else {
        // Scan a single port
        verboseLog(verbose, "Scanning " + std::string(use_tcp ? "TCP" : "UDP") + " port "
                  + std::to_string(port) + " on " + host + "...");
        
        bool is_open = scanner.scanPort(port);
        std::cout << "Port " << port << " is " << (is_open ? "open" : "closed") << std::endl;
    }
}

/**
 * @brief Main function
 * 
 * This function is the entry point of the program. It parses command-line
 * arguments and executes the appropriate mode (server, client, or port scanner).
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return 0 on success, non-zero on error
 */
int main(int argc, char* argv[]) {
    // Set up signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigaction(SIGINT, &sa, NULL);
    
    signal(SIGALRM, alarmHandler);
    
    // If no arguments provided, show help
    if (argc == 1) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Default values
    bool listen_mode = false;
    bool use_tcp = true;
    bool use_ipv6 = false;
    bool verbose = false;
    bool scan_mode = false;
    int port = 0;
    int end_port = 0;
    std::string host = "127.0.0.1";
    std::string command;
    
    // Define long options
    static struct option long_options[] = {
        {"ipv6",    no_argument,       0, '6'},
        {"listen",    no_argument,       0, 'l'},
        {"port",      required_argument, 0, 'p'},
        {"udp",       no_argument,       0, 'u'},
        {"scan",      required_argument, 0, 's'},
        {"range",     required_argument, 0, 'r'},
        {"execute",   required_argument, 0, 'e'},
        {"verbose",   no_argument,       0, 'v'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // First pass: Parse options
    int opt;
    int option_index = 0;
    optind = 1;  // Reset getopt
    while ((opt = getopt_long(argc, argv, "lp:us:r:e:vh6", long_options, &option_index)) != -1) {
        switch (opt) {
            case '6':
                use_ipv6 = true;
                break;
            case 'l':
                listen_mode = true;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'u':
                use_tcp = false;
                break;
            case 's':
                scan_mode = true;
                port = std::stoi(optarg);
                break;
            case 'r': {
                scan_mode = true;
                std::string range = optarg;
                size_t dash_pos = range.find('-');
                if (dash_pos != std::string::npos) {
                    port = std::stoi(range.substr(0, dash_pos));
                    end_port = std::stoi(range.substr(dash_pos + 1));
                }
                break;
            }
            case 'e':
                command = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Handle non-option arguments
    std::vector<std::string> non_option_args;
    for (int i = optind; i < argc; i++) {
        non_option_args.push_back(argv[i]);
    }
    
    // Process non-option arguments
    if (!non_option_args.empty()) {
        // First non-option argument could be host or port
        if (isNumber(non_option_args[0])) {
            // It's a port number
            port = std::stoi(non_option_args[0]);
            
            if (non_option_args.size() > 1) {
                // Second argument is host
                host = non_option_args[1];
            }
        } else {
            // It's a hostname
            host = non_option_args[0];
            
            if (non_option_args.size() > 1 && isNumber(non_option_args[1])) {
                // Second argument is port
                port = std::stoi(non_option_args[1]);
            }
        }
    }
    
    // Check for -e option in the original arguments
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "-e") {
            command = argv[i + 1];
            break;
        }
    }
    
    // Validate port
    if (port != 0 && (port <= 0 || port > 65535)) {
        std::cerr << "Invalid port number. Port must be between 1 and 65535." << std::endl;
        return 1;
    }
    
    // Execute the appropriate mode
    try {
        // Check if -e flag is used with UDP mode
        if (!use_tcp && !command.empty()) {
            std::cerr << "Error: The -e flag cannot be used with UDP mode." << std::endl;
            std::cerr << "Process execution is only supported with TCP connections." << std::endl;
            return 1;
        }
        
        if (scan_mode) {
            // Port scanning mode
            scanPorts(host, port, end_port, use_tcp, verbose, use_ipv6);
        } else if (listen_mode) {
            // Server mode
            if (port == 0) {
                std::cerr << "Error: No port specified for listen mode." << std::endl;
                return 1;
            }
            
            // Make a copy of the command to ensure it's not modified
            std::string server_command = command;
            
            if (use_tcp) {
                runTCPServer(port, verbose, use_ipv6, server_command);
            } else {
                runUDPServer(port, verbose, use_ipv6, "");  // Empty command for UDP
            }
        } else if (port != 0) {
            // Client mode
            if (use_tcp) {
                runTCPClient(host, port, verbose, use_ipv6);
            } else {
                runUDPClient(host, port, verbose, use_ipv6);
            }
        } else {
            // Invalid arguments
            std::cerr << "Invalid arguments. Use -h for help." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
