#ifndef PROCESS_SERVER_H
#define PROCESS_SERVER_H

#include "socket_handler.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>

/**
 * @class ProcessServer
 * @brief Class for executing a process and redirecting its I/O to a socket
 *
 * This class allows executing a command in a child process and redirecting
 * its standard input, output, and error streams to a socket. This enables
 * remote execution of commands, similar to a remote shell.
 */
class ProcessServer {
private:
    pid_t process_id_ = -1;     ///< PID of the child process
    int stdin_pipe_[2];         ///< Pipe for writing to child's stdin
    int stdout_pipe_[2];        ///< Pipe for reading from child's stdout
    int stderr_pipe_[2];        ///< Pipe for reading from child's stderr
    
    std::atomic<bool> running_{false}; ///< Whether child process is running
    
    /**
     * Threads that do I/O between the socket and the process:
     * - One thread sends data from the socket to process stdin
     * - One or more threads send data from process stdout/stderr to the socket
     */
    std::vector<std::thread> io_threads_;
    
    /**
     * @brief Handle data flow from socket to process stdin
     * @param socket Socket to read data from
     * @param write_fd File descriptor to write data to (process stdin)
     */
    void handleSocketToProcess(SocketHandler& socket, int write_fd);
    
    /**
     * @brief Handle data flow from process stdout/stderr to socket
     * @param read_fd File descriptor to read data from (process stdout/stderr)
     * @param socket Socket to write data to
     */
    void handleProcessToSocket(int read_fd, SocketHandler& socket);
    
    /**
     * @brief Signal handler for child process termination
     * @param signal Signal number
     */
    static void handleSignal(int signal);
    
    /**
     * @brief Pointer to the current ProcessServer instance
     * 
     * Used by the signal handler to access instance data (e.g., process_id_, running_)
     */
    static ProcessServer* instance_;

public:
    /**
     * @brief Constructor
     */
    ProcessServer();
    
    /**
     * @brief Destructor
     */
    ~ProcessServer();
    
    /**
     * @brief Start a process and redirect I/O to/from socket
     * @param command Command to execute
     * @param socket Socket to redirect I/O to/from
     * @return true if process was started successfully, false otherwise
     */
    bool startProcess(const std::string& command, SocketHandler& socket);
    
    /**
     * @brief Stop the process
     */
    void stopProcess();
    
    /**
     * @brief Check if the process is running
     * @return true if the process is running, false otherwise
     */
    bool isRunning() const;
};

inline bool ProcessServer::isRunning() const {
    return running_;
}

// Reads from socket (i.e. remote client), writes to process's stdin
inline void ProcessServer::handleSocketToProcess(SocketHandler& socket, int write_fd) {
    const size_t buffer_size = 4096;
    char buffer[buffer_size];
    
    while (running_) {
        // Receive data from socket
        int bytes_read = socket.receive(buffer, buffer_size);
        
        if (bytes_read > 0) {
            // Write data to process stdin
            write(write_fd, buffer, bytes_read);
        } else if (bytes_read == 0) {
            // Socket closed
            break;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
            // If it's a temporary read error, wait and retry
            usleep(10000);  // 10ms
        }
    }
}

// Reads from process's stdout, sends to socket (i.e. remote client)
inline void ProcessServer::handleProcessToSocket(int read_fd, SocketHandler& socket) {
    char buffer[4096];
    
    // Read until EOF on the child's pipe
    while (true) {
        int bytes_read = read(read_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            socket.send(buffer, bytes_read);
            continue;
        } else if (bytes_read == 0) {  
            // If the process closed the pipe (EOF), exit the loop
            break;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) { 
            // If resource temporarily unavailable, retry after a short pause
            usleep(10000); 
            continue;
        } else { 
            // On any other read error, stop forwarding
            break;
        }
    }
}

// Handles SIGCHLD - the signal sent when a process exits
inline void ProcessServer::handleSignal(int signal) {
    if (signal == SIGCHLD && instance_ != nullptr) {
        int status;
        
        // Wait for a child process to change state (usually to exit), and to retrieve its exit status.
        // WNOHANG makes it non-blocking (so the program doesn't freeze if nothing exited yet)
        pid_t pid = waitpid(-1, &status, WNOHANG);
        
        if (pid == instance_->process_id_) {
            // Child process terminated
            instance_->process_id_ = -1;
            instance_->running_ = false;
        }
    }
}

#endif
