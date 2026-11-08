
#include "aiws/gui/remote_add_server.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <QFile>
#include <QFileDevice>
#include <QMetaObject>
#include <QString>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>

// the socket server runs on its own thread and sends accepted paths to the gui
namespace aiws::gui {

RemoteAddServer::RemoteAddServer(
    QObject* receiver, const std::string& socket_path)
    : receiver_(receiver), socket_path_(socket_path) {
}

// stops the server before its resources are destroyed
RemoteAddServer::~RemoteAddServer() {
    stop();
}

// creates the listening socket and starts the server thread
bool RemoteAddServer::start() {
    if (running_.load()) {
        return false;
    }

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd_ < 0) {
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    if (socket_path_.empty() ||
        socket_path_.size() >= sizeof(address.sun_path)) {
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::strncpy(address.sun_path, socket_path_.c_str(),
                 sizeof(address.sun_path) - 1);

    // refuse to overwrite an existing socket path
    if (access(socket_path_.c_str(), F_OK) == 0) {
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (bind(server_fd_,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 8) < 0) {
        close(server_fd_);
        server_fd_ = -1;
        unlink(socket_path_.c_str());
        return false;
    }

    running_.store(true);
    server_thread_ = std::thread(&RemoteAddServer::serve, this);
    return true;
}

// stops accepting requests and joins the server thread
void RemoteAddServer::stop() {
    running_.store(false);

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
        unlink(socket_path_.c_str());
    }
}

// reads an entire message even if it arrives in several pieces
bool RemoteAddServer::receive_all(
    int fd, void* data, std::size_t size) {

    char* bytes = static_cast<char*>(data);
    std::size_t received = 0;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(5);

    while (received < size && running_.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        pollfd item{};
        item.fd = fd;
        item.events = POLLIN;

        int result = poll(&item, 1, 50);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            continue;
        }

        ssize_t count = recv(fd, bytes + received,
                             size - received, 0);

        if (count < 0 && errno == EINTR) {
            continue;
        }

        if (count <= 0) {
            return false;
        }

        received += static_cast<std::size_t>(count);
    }

    return received == size;
}

// validates the document and replies without waiting for the gui
void RemoteAddServer::handle_client(int client_fd) {
    std::uint32_t network_length = 0;

    if (!receive_all(client_fd, &network_length,
                     sizeof(network_length))) {
        return;
    }

    std::uint32_t length = ntohl(network_length);

    if (length == 0 || length > 1024 * 1024) {
        char response = '1';
        send(client_fd, &response, 1, MSG_NOSIGNAL);
        return;
    }

    std::string path(length, '\0');

    if (!receive_all(client_fd, path.data(), path.size())) {
        return;
    }

    QString file_path = QString::fromUtf8(
        path.data(), static_cast<qsizetype>(path.size())
    );

    QFile file(file_path);

    bool readable = file.open(QIODevice::ReadOnly);

    if (readable) {
        file.readAll();
        readable = file.error() == QFileDevice::NoError;
        file.close();
    }

    char response = readable ? '0' : '1';

    // send the reply directly from the server thread
    send(client_fd, &response, 1, MSG_NOSIGNAL);

    if (readable && running_.load()) {
        QMetaObject::invokeMethod(
            receiver_,
            "remote_add_document",
            Qt::QueuedConnection,
            Q_ARG(QString, file_path)
        );
    }
}

// accepts incoming connections without blocking the gui thread
void RemoteAddServer::serve() {
    while (running_.load()) {
        pollfd item{};
        item.fd = server_fd_;
        item.events = POLLIN;

        int result = poll(&item, 1, 50);

        if (result <= 0) {
            continue;
        }

        int client_fd = accept(server_fd_, nullptr, nullptr);

        if (client_fd < 0) {
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }
}

}  // namespace aiws::gui

