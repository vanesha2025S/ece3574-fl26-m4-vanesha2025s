
#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineParser>
#include <QCommandLineOption>

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

    parser.addOption(build_delay_option);
    parser.process(app);

    bool valid_delay = false;
    int delay = parser.value(build_delay_option).toInt(&valid_delay);

    if (!valid_delay || delay < 0) {
        qCritical("Invalid --build-delay-ms value.");
        return 1;
    }

    aiws::gui::MainWindow window;
    window.set_build_delay_ms(delay);

    window.set_exit_callback(&confirm_exit);
    window.show();

    return app.exec();
}
