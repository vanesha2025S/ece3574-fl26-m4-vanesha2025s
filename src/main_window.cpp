#include "aiws/gui/main_window.hpp"
#include "aiws/gui/workspace_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/resettable.hpp"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <QMetaObject>
#include <utility>
namespace aiws::gui {
// Function-pointer callback for the exit button
void confirm_exit(QWidget* parent) {
    auto answer = QMessageBox::question(
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
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("AI Workspace");
    resize(900, 900);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* main_layout = new QVBoxLayout(central);

    auto* title_label = new QLabel("AI Workspace", this);
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setStyleSheet(
        "background-color: #245a81;"
        "color: white;"
        "font-size: 20px;"
        "font-weight: bold;"
        "padding: 12px;"
    );

    main_layout->addWidget(title_label);

    workspace_widget_ = new WorkspaceWidget(this);
    search_widget_ = new SearchWidget(this);
    term_inspector_widget_ = new TermInspectorWidget(this);
    context_widget_ = new ContextWidget(this);

    main_layout->addWidget(workspace_widget_, 2);
    main_layout->addWidget(search_widget_, 3);

    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->addWidget(term_inspector_widget_, 1);
    bottom_layout->addWidget(context_widget_, 1);

    main_layout->addLayout(bottom_layout, 2);

    status_label_ = new QLabel("Ready.", this);
    reset_button_ = new QPushButton("Reset", this);
    exit_button_ = new QPushButton("Exit", this);

    reset_button_->setStyleSheet(
        "background-color: #e4b45b;"
        "color: black;"
        "padding: 8px;"
    );

    exit_button_->setStyleSheet(
        "background-color: #ba5555;"
        "color: white;"
        "padding: 8px;"
    );

    auto* status_layout = new QHBoxLayout();
    status_layout->addWidget(status_label_, 1);
    status_layout->addWidget(reset_button_);
    status_layout->addWidget(exit_button_);

    main_layout->addLayout(status_layout);

    // Workspace signals
    connect(workspace_widget_,&WorkspaceWidget::add_document_requested,this, &MainWindow::add_document);

    connect(workspace_widget_,&WorkspaceWidget::build_corpus_requested,this, &MainWindow::build_corpus);

    connect(workspace_widget_,&WorkspaceWidget::document_selected,this, &MainWindow::document_selected);

    // Search signals
    connect(search_widget_,&SearchWidget::search_requested,this, &MainWindow::search_requested);

    connect(search_widget_, &SearchWidget::query_changed,this, &MainWindow::query_changed);

    connect(search_widget_,&SearchWidget::source_document_requested, this, &MainWindow::source_document_requested);

    connect(search_widget_,&SearchWidget::selected_chunk_changed,term_inspector_widget_,&TermInspectorWidget::set_selected_chunk);

    // Term Inspector signal
    connect(term_inspector_widget_,&TermInspectorWidget::term_inspection_requested,this, &MainWindow::term_inspection_requested);

    // Context signal
    connect(context_widget_,&ContextWidget::build_context_requested,this, &MainWindow::build_context_requested);

    // Reset and Exit
    connect(reset_button_, &QPushButton::clicked,this, &MainWindow::reset_application);

    connect(exit_button_, &QPushButton::clicked,this, &MainWindow::exit_application);

    set_exit_callback(&confirm_exit);
    update_action_states();
}


MainWindow::~MainWindow() {
    build_cancelled_.store(true);

    if (build_thread_.joinable()) {
        build_thread_.join();
    }
}

void MainWindow::set_build_delay_ms(int ms_per_document) {
    build_delay_ms_ = std::max(0, ms_per_document);
}


void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    exit_callback_ = callback;
}


void MainWindow::add_document() {
    QString path = QFileDialog::getOpenFileName(
        this,
        "Add Document",
        QString(),
        "Text Files (*.txt);;All Files (*)"
    );

    if (path.isEmpty()) {
        return;
    }

    if (!add_document_file(path)) {
        QMessageBox::warning(
            this,
            "File Error",
            "Could not open the selected file."
        );
    }
}

bool MainWindow::add_document_file(const QString& path) {
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString text = QString::fromUtf8(file.readAll());

    if (file.error() != QFileDevice::NoError) {
        return false;
    }

    QFileInfo info(path);
    QString absolute_path = info.absoluteFilePath();
    QString title = info.fileName();

    Document document(
        absolute_path.toStdString(),
        title.toStdString(),
        text.toStdString()
    );

    workspace_.add_document(std::move(document));
    workspace_widget_->add_document(title);

    invalidate_corpus();
    status_label_->setText("Document added: " + title);

    return true;
}


void MainWindow::build_corpus() {
    if (workspace_.documents().empty() || build_running_) {
        return;
    }

    if (build_thread_.joinable()) {
        build_thread_.join();
    }

    Workspace snapshot = workspace_;
    build_generation_ = workspace_generation_;
    int delay = build_delay_ms_;

    build_cancelled_.store(false);
    build_running_ = true;
    corpus_current_ = false;
    pending_core_.reset();
    pending_error_.clear();

    clear_processing_outputs();
    workspace_widget_->set_corpus_status("Building...");
    status_label_->setText("Building corpus...");
    update_action_states();

    
    build_thread_ = std::thread(
        [this, snapshot = std::move(snapshot), delay]() mutable {
            try {
                auto core = std::make_unique<ProcessingCore>();
                bool cancelled = false;

                for (std::size_t i = 0; i < snapshot.documents().size(); ++i) {
                    int elapsed = 0;

                    while (elapsed < delay && !build_cancelled_.load()) {
                        int interval = std::min(10, delay - elapsed);
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(interval)
                        );
                        elapsed += interval;
                    }

                    if (build_cancelled_.load()) {
                        cancelled = true;
                        break;
                    }
                }

                if (!cancelled && !build_cancelled_.load()) {
                    core->rebuild(snapshot);
                    pending_core_ = std::move(core);
                }

            } catch (const std::exception& e) {
                pending_error_ = e.what();
            } catch (...) {
                pending_error_ = "Unknown build error.";
            }

        #ifndef AIWS_FAULT_LOSE_FAILURE
            QMetaObject::invokeMethod(
                this,
                "build_finished",
                Qt::QueuedConnection
            );
        #else
            if (pending_error_.empty()) {
                QMetaObject::invokeMethod(
                    this,
                    "build_finished",
                    Qt::QueuedConnection
                );
            }
        #endif
        }
    );

}


void MainWindow::build_finished() {
    if (build_thread_.joinable()) {
        build_thread_.join();
    }

    build_running_ = false;

    #ifndef AIWS_FAULT_ACCEPT_STALE
        if (build_generation_ != workspace_generation_) {
    #else
        if (false) {
    #endif
        pending_core_.reset();
        pending_error_.clear();
        corpus_current_ = false;

        workspace_widget_->set_corpus_status("Not built");
        status_label_->setText("Workspace changed. Rebuild required.");

        update_action_states();
        return;
    }

    if (!pending_error_.empty()) {
        pending_core_.reset();
        corpus_current_ = false;

        workspace_widget_->set_corpus_status("Failed");
        status_label_->setText(
            QString::fromStdString(pending_error_)
        );

        update_action_states();
        return;
    }

    if (pending_core_) {
        processing_core_ = std::move(*pending_core_);
        pending_core_.reset();

        corpus_current_ = true;
        clear_processing_outputs();

        workspace_widget_->set_corpus_status(
            QString::number(processing_core_.chunk_count()) + " chunks"
        );

        status_label_->setText("Corpus built.");
    } else {
        corpus_current_ = false;
        workspace_widget_->set_corpus_status("Not built");
        status_label_->setText("Build cancelled.");
    }

    update_action_states();
}



void MainWindow::document_selected(int row) {
    const auto& documents = workspace_.documents();

    if (row < 0 || static_cast<std::size_t>(row) >= documents.size()) {
        workspace_widget_->clear_selected_document();
        return;
    }

    const Document& document = documents[row];

    workspace_widget_->show_selected_document(
        QString::fromStdString(document.title()),
        QString::fromStdString(document.text())
    );
}

void MainWindow::search_requested(const QString& query, int k) {
     if (!corpus_current_ || query.trimmed().isEmpty()) {
        return;
    }

    auto results = processing_core_.search(
        query.toStdString(), k
    );

    search_widget_->set_results(std::move(results));
    context_widget_->clear_context();
    term_inspector_widget_->clear_statistics();

    status_label_->setText("Search completed.");
}

void MainWindow::query_changed(const QString& query) {
    (void)query;
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    if (!corpus_current_ || !search_widget_->has_query()) {
        return;
    }

    auto items = processing_core_.build_context(
        search_widget_->query().toStdString(),
        search_widget_->result_count(),
        static_cast<std::size_t>(token_budget)
    );

    context_widget_->set_context(items);
    status_label_->setText("Context built.");
}

void MainWindow::source_document_requested(const QString& document_id) {
    const auto& documents = workspace_.documents();

    for (std::size_t i = 0; i < documents.size(); ++i) {
        if (QString::fromStdString(documents[i].id())== document_id) {

            workspace_widget_->select_document(
                static_cast<int>(i)
            );

            document_selected(static_cast<int>(i));

            status_label_->setText("Source document selected.");
            return;
        }
    }
}

void MainWindow::term_inspection_requested(const QString& term,
                                           const QString& chunk_id) {
    if (!corpus_current_ || term.trimmed().isEmpty()) {
        return;
    }

    std::size_t df =
        processing_core_.document_frequency(
            term.toStdString()
        );

    bool has_chunk = !chunk_id.isEmpty();
    std::size_t tf = 0;

    if (has_chunk) {
        tf = processing_core_.term_frequency(
            term.toStdString(),
            chunk_id.toStdString()
        );
    }

    term_inspector_widget_->set_statistics(
        df, has_chunk, tf
    );

    status_label_->setText("Term inspected.");
}

void MainWindow::reset_application() {
    workspace_.clear();
    corpus_current_ = false;
    ++workspace_generation_;

    std::array<Resettable*, 4> components = {
        workspace_widget_,
        search_widget_,
        term_inspector_widget_,
        context_widget_
    };

    for (Resettable* component : components) {
        component->reset();
    }

    update_action_states();
    status_label_->setText("Application reset.");
}

void MainWindow::exit_application() {
    if (exit_callback_ != nullptr) {
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {
    bool has_documents = !workspace_.documents().empty();
    bool can_process = corpus_current_ && search_widget_->has_query();

    workspace_widget_->set_build_corpus_enabled(
    has_documents && !build_running_);

    search_widget_->set_search_enabled(can_process);

    term_inspector_widget_->set_inspection_enabled(
        corpus_current_
    );

    context_widget_->set_build_context_enabled(
        can_process
    );
}

void MainWindow::invalidate_corpus() {
    corpus_current_ = false;
    ++workspace_generation_;

    workspace_widget_->set_corpus_status(
    build_running_ ? "Building..." : "Not built"
    );

    clear_processing_outputs();
    update_action_states();
}

void MainWindow::clear_processing_outputs() {
    search_widget_->clear_results();
    term_inspector_widget_->set_selected_chunk(QString());
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context();
}

}  // namespace aiws::gui
