
#pragma once

#include <QObject>

#include <atomic>
#include <string>
#include <thread>

namespace aiws::gui {

class RemoteAddServer {
public:
    RemoteAddServer(QObject* receiver, const std::string& socket_path);
    ~RemoteAddServer();

    bool start();
    void stop();

private:
    void serve();
    void handle_client(int client_fd);
    bool receive_all(int fd, void* data, std::size_t size);

    QObject* receiver_{nullptr};
    std::string socket_path_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int server_fd_{-1};
};

}  // namespace aiws::gui
