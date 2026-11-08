
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

// sends all bytes even when send only writes part of the data
bool send_all(int fd, const void* data, std::size_t length) {
    const char* bytes = static_cast<const char*>(data);
    std::size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(
            fd, bytes + sent, length - sent, MSG_NOSIGNAL
        );

        if (result <= 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

// reads all requested bytes even when data arrives in pieces
bool receive_all(int fd, void* data, std::size_t length) {
    char* bytes = static_cast<char*>(data);
    std::size_t received = 0;

    while (received < length) {
        ssize_t result = recv(
            fd, bytes + received, length - received, 0
        );

        if (result <= 0) {
            return false;
        }

        received += static_cast<std::size_t>(result);
    }

    return true;
}

// returns the default socket path for the current user
std::string default_socket_path() {
    const char* user = std::getenv("USER");

    if (user != nullptr && user[0] != '\0') {
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }

    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
}

// connects to the running application and sends a file path
int send_add_request(const std::string& socket_path,
                     const std::string& file_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        std::cerr << "Could not create socket.\n";
        return 2;
    }

    timeval timeout{};
    timeout.tv_sec = 2;

    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
               &timeout, sizeof(timeout));

    timeout.tv_sec = 5;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
               &timeout, sizeof(timeout));

    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    if (socket_path.size() >= sizeof(address.sun_path)) {
        std::cerr << "Socket path is too long.\n";
        close(fd);
        return 2;
    }

    std::strncpy(
        address.sun_path,
        socket_path.c_str(),
        sizeof(address.sun_path) - 1
    );

    if (connect(fd, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) < 0) {
        std::cerr << "Could not connect to AI Workspace.\n";
        close(fd);
        return 2;
    }

    std::uint32_t length =
        htonl(static_cast<std::uint32_t>(file_path.size()));

    if (!send_all(fd, &length, sizeof(length)) ||
        !send_all(fd, file_path.data(), file_path.size())) {
        std::cerr << "Failed to send request.\n";
        close(fd);
        return 2;
    }

    char response = 0;

    if (!receive_all(fd, &response, 1)) {
        std::cerr << "Failed to receive response.\n";
        close(fd);
        return 2;
    }

    close(fd);

    if (response == '0') {
        std::cout << "Document accepted.\n";
        return 0;
    }

    if (response == '1') {
        std::cerr << "Document rejected: unreadable file.\n";
        return 1;
    }

    std::cerr << "Invalid server response.\n";
    return 2;
}

// parses command line arguments and sends the add request
int main(int argc, char* argv[]) {
    std::string socket_path = default_socket_path();
    std::string file_path;

    int index = 1;

    if (index < argc && std::string(argv[index]) == "--socket") {
        if (index + 1 >= argc) {
            std::cerr << "Missing socket path.\n";
            return 2;
        }

        socket_path = argv[index + 1];
        index += 2;
    }

    if (index + 1 >= argc ||
        std::string(argv[index]) != "add" ||
        index + 2 != argc) {
        std::cerr
            << "Usage: aiws_send [--socket PATH] add FILE\n";
        return 2;
    }

    file_path = argv[index + 1];

    if (file_path.empty()) {
        std::cerr << "File path cannot be empty.\n";
        return 1;
    }

    return send_add_request(socket_path, file_path);
}
