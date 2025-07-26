#include "process_server.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>

ProcessServer* ProcessServer::instance_ = nullptr;

ProcessServer::ProcessServer() {
    instance_ = this;
    // Set up signal handler for child process termination
    signal(SIGCHLD, handleSignal);
}

ProcessServer::~ProcessServer() {
    stopProcess();
    instance_ = nullptr;
}

bool ProcessServer::startProcess(const std::string& command, SocketHandler& socket) {
    // If a process is already running, stop it
    if (running_) {
        stopProcess();
    }
    
    // Create pipes for stdin, stdout, stderr
    if (pipe(stdin_pipe_) < 0 || pipe(stdout_pipe_) < 0 || pipe(stderr_pipe_) < 0) {
        std::cerr << "Failed to create pipes: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Duplicate the current process: now we have parent (original) and child (new) processes.
    process_id_ = fork();
    
    if (process_id_ < 0) {
        // Fork failed
        std::cerr << "Fork failed: " << strerror(errno) << std::endl;
        return false;
    } else if (process_id_ == 0) {
        // Child process
        
        // Close unused pipe ends
        close(stdin_pipe_[1]);   // Close write end of stdin pipe
        close(stdout_pipe_[0]);  // Close read end of stdout pipe
        close(stderr_pipe_[0]);  // Close read end of stderr pipe
        
        /* 
            Redirects: 
                stdin from read end of stdin_pipe_
                stdout to the write end of stdout_pipe_
                stderr to the write end of stderr_pipe_
        */
        dup2(stdin_pipe_[0], STDIN_FILENO);
        dup2(stdout_pipe_[1], STDOUT_FILENO);
        dup2(stderr_pipe_[1], STDERR_FILENO);
        
        // Close the duplicated descriptors
        close(stdin_pipe_[0]);
        close(stdout_pipe_[1]);
        close(stderr_pipe_[1]);
        
        // Execute the command
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        
        // If execl returns, there was an error
        std::cerr << "Exec failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        
        // Close unused pipe ends
        close(stdin_pipe_[0]);   // Close read end of stdin pipe
        close(stdout_pipe_[1]);  // Close write end of stdout pipe
        close(stderr_pipe_[1]);  // Close write end of stderr pipe
        
        // Set pipes to non-blocking mode (the threads don't freeze if there's no data) 
        fcntl(stdin_pipe_[1], F_SETFL, fcntl(stdin_pipe_[1], F_GETFL, 0) | O_NONBLOCK);
        fcntl(stdout_pipe_[0], F_SETFL, fcntl(stdout_pipe_[0], F_GETFL, 0) | O_NONBLOCK);
        fcntl(stderr_pipe_[0], F_SETFL, fcntl(stderr_pipe_[0], F_GETFL, 0) | O_NONBLOCK);
        
        running_ = true;
        
        // Create threads to handle I/O:
        io_threads_.emplace_back(&ProcessServer::handleSocketToProcess, this, std::ref(socket), stdin_pipe_[1]);
        io_threads_.emplace_back(&ProcessServer::handleProcessToSocket, this, stdout_pipe_[0], std::ref(socket));
        io_threads_.emplace_back(&ProcessServer::handleProcessToSocket, this, stderr_pipe_[0], std::ref(socket));
        
        return true;
    }
}

void ProcessServer::stopProcess() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Wait for I/O threads to finish
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    io_threads_.clear();
    
    // Kill the process if it's still running
    if (process_id_ > 0) {
        kill(process_id_, SIGTERM);
        
        int status;
        // Wait for the process to terminate
        waitpid(process_id_, &status, 0);
        
        process_id_ = -1;
    }
    
    // Close any open pipe ends
    close(stdin_pipe_[1]);
    close(stdout_pipe_[0]);
    close(stderr_pipe_[0]);
}