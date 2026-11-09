// Your M4 tests (specification, Section 6).


 // Your M4 tests (specification, Section 6).

#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using aiws::gui::MainWindow;

int failures = 0;

// checks the condition and prints whether the test passed or failed
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    } else {
        std::cout << "PASS: " << message << std::endl;
    }
}

// creates a temporary text file for testing
QString create_file(const QString& directory,
                    const QString& name,
                    const QByteArray& text) {
    QString path = directory + "/" + name;
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }

    if (file.write(text) != text.size()) {
        return QString();
    }

    file.close();
    return path;
}

// gets the current corpus status from the gui
QString corpus_status(MainWindow& window) {
    auto labels = window.findChildren<QLabel*>();

    for (QLabel* label : labels) {
        if (label->text().startsWith("Corpus:")) {
            return label->text();
        }
    }

    return QString();
}

// checks whether any gui label displays the requested message
bool has_status_message(MainWindow& window, const QString& message) {
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (label->text() == message) {
            return true;
        }
    }
    return false;
}

// gets the number of documents shown in the workspace list
int document_count(MainWindow& window) {
    auto* list = window.findChild<QListWidget*>();
    return list == nullptr ? -1 : list->count();
}

// waits for a condition to become true or until the timeout
bool wait_until(const std::function<bool()>& condition,
                int timeout_ms = 2000) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        if (condition()) {
            return true;
        }

        QTest::qWait(10);
    }

    return condition();
}

// starts a corpus build through the main window
bool start_build(MainWindow& window) {
    return QMetaObject::invokeMethod(
        &window,
        "build_corpus",
        Qt::DirectConnection
    );
}

// tests adding valid and invalid documents
void test_add_document(const QString& path) {
    std::cout << "\nTesting Add Document..." << std::endl;

    MainWindow window;

    check(
        !window.add_document_file(path + "_missing"),
        "Unreadable file is rejected"
    );

    check(
        corpus_status(window) == "Corpus: Not built",
        "Invalid file does not change corpus status"
    );

    check(
        window.add_document_file(path),
        "Valid document is added"
    );

    check(
        corpus_status(window) == "Corpus: Not built",
        "New document requires a corpus build"
    );
}

// tests whether the corpus builds successfully in the background
void test_successful_build(const QString& path) {
    std::cout << "\nTesting Background Build..." << std::endl;

    MainWindow window;
    window.set_build_delay_ms(200);

    check(window.add_document_file(path), "Document added");
    check(start_build(window), "Build request accepted");

    check(
        corpus_status(window) == "Corpus: Building...",
        "Building state appears immediately"
    );

    bool completed = wait_until([&window]() {
        return corpus_status(window).endsWith(" chunks");
    });

    check(completed, "Background build finishes");

    check(
        corpus_status(window).endsWith(" chunks"),
        "Completed build displays chunk count"
    );
}

// tests whether an outdated build is rejected after adding a document
void test_stale_build(const QString& first,
                      const QString& second) {
    std::cout << "\nTesting Stale Build..." << std::endl;

    MainWindow window;
    window.set_build_delay_ms(300);

    check(window.add_document_file(first), "First document added");
    check(start_build(window), "Build started");

    check(
        window.add_document_file(second),
        "Second document added during build"
    );

    bool completed = wait_until([&window]() {
        return has_status_message(
            window, "Workspace changed. Rebuild required."
        ) || corpus_status(window).endsWith(" chunks") ||
            corpus_status(window) == "Corpus: Failed";
    });

    check(completed, "Old build finishes");

    check(
        corpus_status(window) == "Corpus: Not built" &&
        has_status_message(window, "Workspace changed. Rebuild required."),
        "Outdated build is rejected"
    );
}

// tests whether reset prevents an old build from becoming current
void test_reset_during_build(const QString& path) {
    std::cout << "\nTesting Reset During Build..." << std::endl;

    MainWindow window;
    window.set_build_delay_ms(300);

    check(window.add_document_file(path), "Document added");
    check(start_build(window), "Build started");

    check(
        QMetaObject::invokeMethod(
            &window,
            "reset_application",
            Qt::DirectConnection
        ),
        "Reset invoked"
    );

    bool finished = wait_until([&window]() {
        return has_status_message(
            window, "Workspace changed. Rebuild required."
        ) || corpus_status(window).endsWith(" chunks") ||
            corpus_status(window) == "Corpus: Failed";
    });

    check(finished, "Old build completes after Reset");

    check(
        corpus_status(window) == "Corpus: Not built" &&
        document_count(window) == 0,
        "Reset prevents old corpus from becoming current"
    );
}

// tests whether a failed build is reported to the gui
void test_build_failure(const QString& path) {
    std::cout << "\nTesting Build Failure..." << std::endl;

    MainWindow window;
    window.set_build_delay_ms(0);

    check(window.add_document_file(path), "First document added");

    check(
        window.add_document_file(path),
        "Duplicate document added to trigger build failure"
    );

    check(start_build(window), "Build started");

    bool completed = wait_until([&window]() {
        return corpus_status(window) != "Corpus: Building...";
    });

    check(completed, "Failed build leaves Building state");

    check(
        corpus_status(window) == "Corpus: Failed",
        "Build failure is reported to GUI"
    );
}

// tests whether the gui stays responsive during a delayed build
void test_responsiveness(const QString& path) {
    std::cout << "\nTesting GUI Responsiveness..." << std::endl;

    MainWindow window;
    window.set_build_delay_ms(500);

    check(window.add_document_file(path), "Document added");
    check(start_build(window), "Delayed build started");

    QLineEdit* input = window.findChild<QLineEdit*>();

    check(input != nullptr, "GUI input exists");

    if (input != nullptr) {
        input->setText("testing responsiveness");

        check(
            input->text() == "testing responsiveness",
            "GUI input remains editable during build"
        );
    }

    bool completed = wait_until([&window]() {
        return corpus_status(window).endsWith(" chunks");
    });

    check(completed, "Delayed background build completes");
}

// sends every byte of a request to a unix socket
bool socket_send_all(int fd, const char* bytes, std::size_t size) {
    std::size_t sent = 0;

    while (sent < size) {
        ssize_t count = send(fd, bytes + sent, size - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }

    return true;
}

// sends a remote add request with an optional split message
char remote_request(const QString& socket_path,
                    const QString& file_path,
                    bool split = false) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return '?';
    }

    timeval timeout{};
    timeout.tv_sec = 3;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    QByteArray socket_bytes = QFile::encodeName(socket_path);
    if (socket_bytes.size() >= static_cast<int>(sizeof(address.sun_path))) {
        close(fd);
        return '?';
    }
    std::memcpy(address.sun_path, socket_bytes.constData(),
                static_cast<std::size_t>(socket_bytes.size()));

    if (connect(fd, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) != 0) {
        close(fd);
        return '?';
    }

    QByteArray path_bytes = file_path.toUtf8();
    std::uint32_t length = htonl(static_cast<std::uint32_t>(path_bytes.size()));
    std::string message(reinterpret_cast<const char*>(&length), sizeof(length));
    message.append(path_bytes.constData(),
                   static_cast<std::size_t>(path_bytes.size()));

    bool sent = true;
    if (split) {
        std::size_t position = 0;
        const std::size_t pieces[] = {2, 2, 5};
        for (std::size_t piece : pieces) {
            sent = socket_send_all(fd, message.data() + position, piece);
            if (!sent) {
                break;
            }
            position += piece;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
        if (sent) {
            sent = socket_send_all(fd, message.data() + position,
                                   message.size() - position);
        }
    } else {
        sent = socket_send_all(fd, message.data(), message.size());
    }

    char response = '?';
    if (sent) {
        ssize_t count;
        do {
            count = recv(fd, &response, 1, 0);
        } while (count < 0 && errno == EINTR);
        if (count != 1) {
            response = '?';
        }
    }

    close(fd);
    return response;
}

// checks remote add communication and the actual workspace update
void test_remote_add(const QString& directory, const QString& first,
                     const QString& second) {
    std::cout << "\nTesting Remote Add..." << std::endl;

    QString socket_path = directory + "/remote.sock";
    MainWindow window;

    check(window.start_remote_add_server(socket_path),
          "Remote add server starts");
    check(QFile::exists(socket_path), "Remote socket exists");

    auto accepted = std::async(std::launch::async, [&]() {
        return remote_request(socket_path, first);
    });
    bool reply_ready = wait_until([&]() {
        return accepted.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    });
    check(reply_ready && accepted.get() == '0',
          "Readable document is accepted remotely");

    check(wait_until([&]() { return document_count(window) == 1; }),
          "Remote document appears in the workspace");

    auto rejected = std::async(std::launch::async, [&]() {
        return remote_request(socket_path, first + "_missing");
    });
    bool rejection_ready = wait_until([&]() {
        return rejected.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    });
    check(rejection_ready && rejected.get() == '1',
          "Unreadable remote document is rejected");
    check(document_count(window) == 1,
          "Rejected request does not change the workspace");

    window.set_build_delay_ms(400);
    check(start_build(window), "Background build starts before remote add");

    auto second_reply = std::async(std::launch::async, [&]() {
        return remote_request(socket_path, second);
    });
    bool second_ready = wait_until([&]() {
        return second_reply.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    });
    check(second_ready && second_reply.get() == '0',
          "Remote add succeeds during a build");

    check(wait_until([&]() { return document_count(window) == 2; }),
          "Remote document is added during the build");

    bool finished = wait_until([&]() {
        return has_status_message(window, "Workspace changed. Rebuild required.") ||
               corpus_status(window).endsWith(" chunks") ||
               corpus_status(window) == "Corpus: Failed";
    });
    check(finished, "Build finishes after remote workspace change");
    check(corpus_status(window) == "Corpus: Not built" &&
          has_status_message(window, "Workspace changed. Rebuild required."),
          "Remote add invalidates the old corpus");
}

// checks special filenames and a request split into multiple writes
void test_remote_special_paths(const QString& directory) {
    std::cout << "\nTesting Special Remote Paths..." << std::endl;

    QString socket_path = directory + "/special.sock";
    MainWindow window;
    check(window.start_remote_add_server(socket_path),
          "Special-path server starts");

    const QString names[] = {
        "file with spaces.txt",
        QString::fromUtf8("caf\xC3\xA9_\xE6\xB5\x8B\xE8\xAF\x95.txt"),
        "line\nbreak.txt"
    };

    for (const QString& name : names) {
        QString path = create_file(directory, name, "remote document\n");
        check(!path.isEmpty(), "Special filename test file created");
        if (path.isEmpty()) {
            continue;
        }

        auto request = std::async(std::launch::async, [&, path]() {
            return remote_request(socket_path, path, true);
        });
        bool ready = wait_until([&]() {
            return request.wait_for(std::chrono::milliseconds(0)) ==
                   std::future_status::ready;
        });
        check(ready && request.get() == '0',
              "Split request with special filename is accepted");
    }

    check(wait_until([&]() { return document_count(window) == 3; }),
          "All special filenames appear in the workspace");
}

// checks that the command line client and server use the same protocol
void test_remote_client(const QString& directory, const QString& path) {
    std::cout << "\nTesting aiws_send Client..." << std::endl;

    QString executable = QCoreApplication::applicationDirPath() + "/aiws_send";
    if (!QFile::exists(executable)) {
        check(false, "aiws_send executable exists in the build directory");
        return;
    }

    QString socket_path = directory + "/client.sock";
    MainWindow window;
    check(window.start_remote_add_server(socket_path),
          "Client test server starts");

    QString file_name = QFileInfo(path).fileName();
    auto client = std::async(std::launch::async, [=]() {
        QProcess process;
        process.setWorkingDirectory(QFileInfo(path).absolutePath());
        process.start(executable, {"--socket", socket_path, "add", file_name});
        if (!process.waitForStarted(1500) ||
            !process.waitForFinished(4000)) {
            process.kill();
            process.waitForFinished(1000);
            return -1;
        }
        return process.exitStatus() == QProcess::NormalExit ?
               process.exitCode() : -1;
    });

    bool ready = wait_until([&]() {
        return client.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }, 5000);
    check(ready && client.get() == 0,
          "aiws_send accepts a relative filename from its working directory");
    check(wait_until([&]() { return document_count(window) == 1; }),
          "aiws_send request updates the workspace");
}

// checks that server destruction closes and removes its socket
void test_remote_cleanup(const QString& directory) {
    std::cout << "\nTesting Remote Server Cleanup..." << std::endl;

    QString socket_path = directory + "/cleanup.sock";
    {
        MainWindow window;
        check(window.start_remote_add_server(socket_path),
              "Cleanup test server starts");
        check(QFile::exists(socket_path), "Socket is present while running");
    }

    check(!QFile::exists(socket_path),
          "Server removes the socket file on shutdown");
}

// creates the test files and runs all the tests
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QTemporaryDir directory;

    if (!directory.isValid()) {
        std::cerr << "Could not create temporary directory."
                  << std::endl;
        return 1;
    }

    QString first = create_file(
        directory.path(),
        "first.txt",
        "AI systems process documents and retrieve information."
    );

    QString second = create_file(
        directory.path(),
        "second.txt",
        "A second document changes the workspace."
    );

    if (first.isEmpty() || second.isEmpty()) {
        std::cerr << "Could not create test documents."
                  << std::endl;
        return 1;
    }

    test_add_document(first);
    test_successful_build(first);
    test_stale_build(first, second);
    test_reset_during_build(first);
    test_build_failure(first);
    test_responsiveness(first);
    test_remote_add(directory.path(), first, second);
    test_remote_special_paths(directory.path());
    test_remote_client(directory.path(), first);
    test_remote_cleanup(directory.path());

    std::cout << "\nTotal failures: " << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
