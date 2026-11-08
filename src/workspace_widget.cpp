#include "aiws/gui/workspace_widget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent) : QWidget(parent) {
    document_list_ = new QListWidget(this);
    add_document_button_ = new QPushButton("Add Document", this);
    build_corpus_button_ = new QPushButton("Build Corpus", this);
    corpus_status_label_ = new QLabel("Corpus: Not built", this);

    document_title_label_ = new QLabel("Selected Document -", this);
    document_text_ = new QPlainTextEdit(this);
    document_text_->setReadOnly(true);
    document_text_->setPlaceholderText("Select a document to view its contents.");

    auto* main_layout = new QHBoxLayout(this);

    auto* workspace_layout = new QVBoxLayout();
    auto* button_layout = new QHBoxLayout();

    button_layout->addWidget(new QLabel("Workspace", this));
    button_layout->addWidget(add_document_button_);
    button_layout->addWidget(build_corpus_button_);

    workspace_layout->addLayout(button_layout);
    workspace_layout->addWidget(document_list_);
    workspace_layout->addWidget(corpus_status_label_);

    auto* document_layout = new QVBoxLayout();
    document_layout->addWidget(document_title_label_);
    document_layout->addWidget(document_text_);

    main_layout->addLayout(workspace_layout, 1);
    main_layout->addLayout(document_layout, 1);

    connect(add_document_button_, &QPushButton::clicked,this, &WorkspaceWidget::add_document_requested);

    connect(build_corpus_button_, &QPushButton::clicked,this, &WorkspaceWidget::build_corpus_requested);

    connect(document_list_, &QListWidget::currentRowChanged,this, &WorkspaceWidget::document_selected);

    reset();
}

void WorkspaceWidget::add_document(const QString& title) {
    document_list_->addItem(title);
    build_corpus_button_->setEnabled(true);
}

void WorkspaceWidget::select_document(int row) {
    if (row < 0 || row >= document_list_->count()) {
        return;
    }

    if (document_list_->currentRow() != row) {
        document_list_->setCurrentRow(row);
    }
}

void WorkspaceWidget::show_selected_document(const QString& title,
                                              const QString& text) {
    document_title_label_->setText("Selected Document - " + title);
    document_text_->setPlainText(text);
}

void WorkspaceWidget::clear_selected_document() {
    document_title_label_->setText("Selected Document -");
    document_text_->clear();
}

void WorkspaceWidget::set_corpus_status(const QString& text) {
    corpus_status_label_->setText("Corpus: " + text);
}

void WorkspaceWidget::set_build_corpus_enabled(bool enabled) {
    build_corpus_button_->setEnabled(enabled);
}

void WorkspaceWidget::reset() {
    QSignalBlocker blocker(document_list_);

    document_list_->clear();
    clear_selected_document();
    set_corpus_status("Not built");
    set_build_corpus_enabled(false);
}

}  // namespace aiws::gui
