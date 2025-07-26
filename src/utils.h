#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>

/**
 * @file utils.h
 * @brief Utility functions for the sockray application
 */

/**
 * @brief Log a message if verbose mode is enabled
 * 
 * This function prints a message to standard output if the verbose flag is set to true.
 * It's used throughout the application to provide additional information during execution.
 * 
 * @param verbose Flag indicating whether verbose mode is enabled
 * @param message The message to log
 */
inline void verboseLog(bool verbose, const std::string& message) {
    if (verbose) {
        std::cout << message << std::endl;
    }
}

#endif // UTILS_H
