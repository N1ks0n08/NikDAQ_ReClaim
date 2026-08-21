#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <iostream>

uint16_t header_parser(uint8_t byte_1, uint8_t byte_2) {
    return (uint16_t(byte_1) << 8) | uint16_t(byte_2);
}

bool echo_bytes(int client_fd, uint8_t* buffer, ssize_t& buf_bytes) {
    // loop contiinously for framing + writing/shifting loop
    while (true) {
        // first check: are there header(s) we can parse?
        if (buf_bytes >= 2) {
            // second check: check if the current message can be used
            uint16_t payload_size = header_parser(buffer[0], buffer[1]);
            const ssize_t frame_len = 2 + payload_size;
            if (buf_bytes >= frame_len) {
                ssize_t total_written_bytes = 0;
                while (true) {
                    // write the message back
                    ssize_t written_bytes;
                    do {
                        written_bytes = write(client_fd, buffer + total_written_bytes, frame_len - total_written_bytes);
                    } while (written_bytes == -1 && errno == EINTR);
                    if (written_bytes == -1) {
                        std::cerr << "Error writing: " << strerror(errno) << std::endl;
                        return false;
                        break;
                    }
                    total_written_bytes += written_bytes;
                    if (total_written_bytes == frame_len) {
                        break;
                    }
                }
                // shift the remaining bytes to the left
                memmove(buffer, buffer + frame_len, buf_bytes - frame_len);
                buf_bytes -= frame_len;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return true;
}

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Error setting socket opt: " << strerror(errno) << std::endl;
        return 1;
    }

    sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(6969);

    if (bind(server_socket_fd, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)) == -1) {
        std::cerr << "Error binding to address: " << strerror(errno) << std::endl;
        return 1;
    }

    if (listen(server_socket_fd, 5) == -1) {
        std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
        return 1;
    }

    while (true) {
        int client_fd;
        do {
            client_fd = accept(server_socket_fd, nullptr, nullptr);
        } while (client_fd == -1 && errno == EINTR);
        if (client_fd == -1) {
            std::cerr << "Error accepting client: " << strerror(errno) << std::endl;
            continue;
        }
        bool connected = true;
        uint8_t buf[66000];
        ssize_t buf_bytes = 0;

        while (connected) {
            ssize_t read_bytes;
            do {
                read_bytes = read(client_fd, buf + buf_bytes, ssize_t(sizeof(buf)) - buf_bytes); // increment buf with buf_bytes to prevent overwriting stored data
                                                                                        // decrement sizeof(buf) by buf_bytes to prevenrt writing more than buffer size
            } while (read_bytes == -1 && errno == EINTR);
            if (read_bytes == 0) {
                if (buf_bytes > 0) {
                    std::cerr << "Client disconnected mid-frame, " << buf_bytes << " bytes discarded." << std::endl;
                } else {
                    std::cout << "Client disconnected!" << std::endl;
                }
                connected = false;
                break;
            }
            if (read_bytes == -1) {
                std::cerr << "Error reading: " << strerror(errno) << std::endl;
                connected = false;
                break;
            }
            buf_bytes += read_bytes;
            if (!echo_bytes(client_fd, buf, buf_bytes)) {
                connected = false;
                break;
            }
        }

        close(client_fd);
    }

    return 0;
}