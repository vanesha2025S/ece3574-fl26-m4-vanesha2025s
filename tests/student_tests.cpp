// Your M4 tests (specification, Section 6).

#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <iostream>

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

    file.write(text);
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
        return corpus_status(window) != "Corpus: Building...";
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
        return corpus_status(window) != "Corpus: Building...";
    });

    check(completed, "Old build finishes");

    check(
        corpus_status(window) == "Corpus: Not built",
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
        auto labels = window.findChildren<QLabel*>();

        for (QLabel* label : labels) {
            if (label->text() ==
                "Workspace changed. Rebuild required.") {
                return true;
            }
        }

        return false;
    });

    check(finished, "Old build completes after Reset");

    check(
        corpus_status(window) == "Corpus: Not built",
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
        return corpus_status(window) != "Corpus: Building...";
    });

    check(completed, "Delayed background build completes");
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

    std::cout << "\nTotal failures: " << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
