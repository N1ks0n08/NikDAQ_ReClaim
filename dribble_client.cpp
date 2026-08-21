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

    /*
    const char* payload = "HELLO WORLD!";
    uint16_t payload_len = uint16_t(strlen(payload));

    uint8_t frame[65537];
    frame[0] = uint8_t(payload_len >> 8);
    frame[1] = uint8_t(payload_len);
    memcpy(frame + 2, payload, payload_len);
    ssize_t frame_len = 2 + payload_len;
    */

    // ---- build two frames into one send buffer ----
    const char* messages[] = { "HELLO", "ABC" };
    const int num_messages = 2;

    uint8_t send_buf[65537 * 2];
    ssize_t send_len = 0;

    for (int m = 0; m < num_messages; m++) {
        uint16_t plen = uint16_t(strlen(messages[m]));
        send_buf[send_len++] = uint8_t(plen >> 8);
        send_buf[send_len++] = uint8_t(plen);
        memcpy(send_buf + send_len, messages[m], plen);
        send_len += plen;
    }

    int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd == -1) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return 1;
    }
    sockaddr_in client_socket_address;
    memset(&client_socket_address, 0, sizeof(client_socket_address));
    client_socket_address.sin_family = AF_INET;
    client_socket_address.sin_port = htons(6969);
    if (inet_pton(AF_INET, "127.0.0.1", &client_socket_address.sin_addr) != 1) {
        std::cerr << "Invalid address" << std::endl;
        return 1;
    }

    if (connect(client_socket_fd, reinterpret_cast<sockaddr*>(&client_socket_address),
                sizeof(client_socket_address)) == -1) {
        std::cerr << "Error connecting: " << strerror(errno) << std::endl;
        return 1;
    }

    
    // ---- WRITE: drain the whole frame ----
    ssize_t total_written_bytes = 0;
    while (total_written_bytes < send_len) {
        ssize_t written;
        do {
            written = write(client_socket_fd, send_buf + total_written_bytes,
                            send_len- total_written_bytes);
        } while (written == -1 && errno == EINTR);
        if (written == -1) {
            std::cerr << "Error writing bytes: " << strerror(errno) << std::endl;
            close(client_socket_fd);
            return 1;
        }
        total_written_bytes += written;
    }

    /*
    // ---- WRITE: dribble one byte at a time ----
    for (ssize_t i = 0; i < send_len; i++) {
        ssize_t written;
        do {
            written = write(client_socket_fd, send_buf + i, 1);
        } while (written == -1 && errno == EINTR);
        if (written == -1) {
            std::cerr << "Error writing byte " << i << ": " << strerror(errno) << std::endl;
            close(client_socket_fd);
            return 1;
        }
        usleep(50000);   // 50ms between bytes
    } */

    // ---- READ: accumulate the echo until we have frame_len bytes ----
    uint8_t reply[65537];
    ssize_t reply_bytes = 0;
    while (reply_bytes < send_len) {
        ssize_t r;
        do {
            // TODO: read into reply at the right offset, with the right capacity.
            //       Not (reply, frame_len) — that overwrites on a fragmented reply.
            r = read(client_socket_fd, reply + reply_bytes, send_len - reply_bytes);
        } while (r == -1 && errno == EINTR);

        if (r == -1) {
            std::cerr << "Error reading: " << strerror(errno) << std::endl;
            close(client_socket_fd);
            return 1;
        }
        if (r == 0) {
            std::cerr << "Server closed before full reply ("
                      << reply_bytes << " of " << send_len << ")" << std::endl;
            close(client_socket_fd);
            return 1;
        }

        // TODO: advance reply_bytes by what you just read.
        reply_bytes += r;
    }

    // ---- VERIFY ----
    // TODO: compare `frame` and `reply` over frame_len bytes; print PASS or FAIL.
    if (reply_bytes == send_len && memcmp(send_buf, reply, send_len) == 0) {
        std::cout << "PASS" << std::endl;
    } else {
        std::cout << "FAIL" << std::endl;
    }

    close(client_socket_fd);
    std::cout << "Finished 2 message coexist test!" << std::endl;
    return 0;
}