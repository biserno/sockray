# Sockray
Sockray is a lightweight command-line network utility for port scanning, socket-based communication, and remote shell access.

### Features
- TCP/UDP port scanning.
- Listen on a specified TCP/UDP port for a connection.
- Connect to a specified TCP/UDP port on a specified host.
- Redirect standard input/output to a network socket.

### Caveats
- Sockray is not compatible with windows; It's designed for Unix-like systems only.
- UDP port scans are unreliable

# Installation
### Prerequisites: 
* C++20 compatible compiler
* CMake 3.10 or higher
* Make

### Building sockray
1\. Clone the repository
```
git clone https://gitlab.mff.cuni.cz/teaching/nprg041/2024-25/svoboda-eng/asadlis.git
cd asadlis/project
```

2\. Create a build directory
```
mkdir build
cd build
```

3\. Run CMake and build the project
```
cmake ..
make
```

4\. Done. The executable will be available as:
`./sockray`

Note: If you encounter permission errors when running `sockray`, you may need to grand execution permission:
`chmod +x sockray`

# Generating Documentation with Doxygen
### Prerequisites:
* Make sure Doxygen is installed on your system.

### Generate Documentation
Following command will generate documentation inside the docs/html/ directory
```
doxygen Doxyfile
```

# Usage Examples
1\. Port scanning
Scan a TCP port on a specified host:
```
./sockray -s 80 127.0.0.1
```

or scan a range of TCP ports:
```
./sockrat -r 1-1000 127.0.0.1
```

2\. Start Remote Shell Server
Listen on a port and provide a remote `/bin/bash` shell:
```
./sockray -l -p 8080 -e "/bin/bash"
```

Then connect from a client:
```
./sockray 127.0.0.1 8080
```


### Help Menu
```
./sockray -h
Usage: ./sockray [options] [hostname] [port]

Options:
  -6, --ipv6                Use IPv6 addresses only.
  -l, --listen              Listen mode (server)
  -p, --port PORT           Specify local port for listen mode
  -u, --udp                 Use UDP instead of TCP
  -s, --scan PORT           Scan a single port
  -r, --range START-END     Scan a range of ports
  -e, --execute COMMAND     Execute command and redirect I/O to network
  -v, --verbose             Verbose mode
  -h, --help                Display this help message

Examples:
  ./sockray localhost 80                  # Connect to localhost port 80
  ./sockray --listen --port 8080          # Listen on port 8080
  ./sockray --udp localhost 53            # UDP connection to localhost port 53
  ./sockray --scan 80 localhost           # Check if port 80 is open
  ./sockray --range 1-1000 localhost      # Scan ports 1-1000
  ./sockray --listen --port 8080 --execute "/bin/bash"  # Set up a remote shell
```
