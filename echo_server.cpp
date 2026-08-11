#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <iostream>


int main() {
    std::signal(SIGPIPE, SIG_IGN);
    // create listening sockets
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::cerr << "socket: " << std::strerror(errno) << "\n";
        return 1;
    }
    // fill in addresses
    sockaddr_in socket_address;
    std::memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_port = htons(3333);
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_family = AF_INET;
    // set address re-use flag
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "setsockopt error: " << strerror(errno) << std::endl;
        return 1;
    }
    // bind the connection
    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)) == -1) {
        std::cerr << "bind failed with error: " << strerror(errno) << std::endl;
        return 1;
    }
    // listen for messages
    if (listen(listen_fd, 5) == -1) {
        std::cerr << "listen failed with error: " << strerror(errno) << std::endl;
        return 1;
    }
    // loop: acccept a client
    while (true) {
        int client_fd;
        do {
            client_fd = accept(listen_fd, nullptr, nullptr);
        } while (client_fd == -1 && errno == EINTR);
        if (client_fd == -1) {
            std::cerr << "accept failed with error: " << strerror(errno) << std::endl;
            continue;
        }
        std::cout << "Client accepted! Client fd: " << client_fd << std::endl;
        bool connected = true;
        //  loop: read bytes
        char buf[1024];
        while (connected) {
            ssize_t read_bytes;
            do {
                read_bytes = read(client_fd, buf, sizeof(buf));
            } while (read_bytes == -1 && errno == EINTR);
            if (read_bytes > 0) {
                ssize_t total_written_bytes = 0;
                while (total_written_bytes < read_bytes) {
                    ssize_t write_bytes;
                    do {
                        write_bytes = write(client_fd, buf + total_written_bytes, read_bytes - total_written_bytes);
                    } while (write_bytes == -1 && errno == EINTR);
                    if (write_bytes == -1) {
                        std::cerr << "write failed with error: " << strerror(errno) << std::endl;
                        connected = false;
                        break;
                    }
                    total_written_bytes += write_bytes;
                }
            }
            else if (read_bytes == 0) {
                std::cout << "Client disconnected!" << std::endl;
                connected = false;
            } else {
                std::cerr << "read failed with error: " << strerror(errno) << std::endl;
                connected = false;
            }
        }

        //  close the client 
        close(client_fd);
    }


    return 0;
}
