
#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include <cstdlib>
#include <string>
#include <unistd.h>

// Exit callback 
void confirm_exit(QWidget* parent) {
    QMessageBox::StandardButton answer =
        QMessageBox::question(
            parent,
            "Confirm Exit",
            "Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

    if (answer == QMessageBox::Yes) {
        QApplication::quit();
    }
}

// returns the default socket path for the current user
QString default_socket_path() {
    const char* user = std::getenv("USER");

    if (user != nullptr && user[0] != '\0') {
        return "/tmp/aiws-" + QString::fromLocal8Bit(user) + ".sock";
    }

    return "/tmp/aiws-" + QString::number(getuid()) + ".sock";
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption build_delay_option(
        "build-delay-ms",
        "Extra build delay per document in milliseconds.",
        "N",
        "0"
    );

    QCommandLineOption ipc_socket_option(
        "ipc-socket",
        "Socket path for remote document addition.",
        "PATH",
        default_socket_path()
    );

    parser.addOption(build_delay_option);
    parser.addOption(ipc_socket_option);
    parser.process(app);

    bool valid_delay = false;
    int delay = parser.value(build_delay_option).toInt(&valid_delay);

    if (!valid_delay || delay < 0) {
        qCritical("Invalid --build-delay-ms value.");
        return 1;
    }

    aiws::gui::MainWindow window;
    window.set_build_delay_ms(delay);

    if (!window.start_remote_add_server(
            parser.value(ipc_socket_option))) {
        qCritical("Could not start the remote add server.");
        return 1;
    }

    window.set_exit_callback(&confirm_exit);
    window.show();

    return app.exec();
}
