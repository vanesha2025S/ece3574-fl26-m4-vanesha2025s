#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/search_result_model.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QString>

#include <utility>

namespace aiws::gui {

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
    query_edit_ = new QLineEdit(this);
    query_edit_->setPlaceholderText("Enter search query");

    result_count_spin_ = new QSpinBox(this);
    result_count_spin_->setRange(1, 100);
    result_count_spin_->setValue(3);

    search_button_ = new QPushButton("Search", this);

    results_view_ = new QTableView(this);
    result_model_ = new SearchResultModel(this);
    results_view_->setModel(result_model_);

    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    results_view_->horizontalHeader()->setStretchLastSection(true);
    results_view_->verticalHeader()->setVisible(false);

    selected_result_text_ = new QPlainTextEdit(this);
    selected_result_text_->setReadOnly(true);
    selected_result_text_->setPlaceholderText("Select a search result to view details.");

    view_source_button_ = new QPushButton("View Source", this);

    auto* main_layout = new QVBoxLayout(this);

    auto* query_layout = new QHBoxLayout();
    query_layout->addWidget(new QLabel("Query:", this));
    query_layout->addWidget(query_edit_, 1);
    query_layout->addWidget(new QLabel("Results (k):", this));
    query_layout->addWidget(result_count_spin_);
    query_layout->addWidget(search_button_);

    main_layout->addLayout(query_layout);

    main_layout->addWidget(new QLabel("Search Results", this));
    main_layout->addWidget(results_view_, 2);

    auto* selected_layout = new QHBoxLayout();
    selected_layout->addWidget(new QLabel("Selected Result", this));
    selected_layout->addWidget(view_source_button_);
    selected_layout->addStretch();

    main_layout->addLayout(selected_layout);
    main_layout->addWidget(selected_result_text_, 1);

    connect(search_button_, &QPushButton::clicked,this, &SearchWidget::request_search);

    connect(query_edit_, &QLineEdit::textChanged,this, &SearchWidget::query_changed);

    connect(results_view_->selectionModel(),&QItemSelectionModel::selectionChanged,this, &SearchWidget::show_selected_result);

    connect(view_source_button_, &QPushButton::clicked,this, &SearchWidget::request_source_document);

    reset();

}

QString SearchWidget::query() const {
    return query_edit_->text();
}

int SearchWidget::result_count() const {
    return result_count_spin_->value();
}

bool SearchWidget::has_query() const {
    return !query().trimmed().isEmpty();
}

void SearchWidget::set_search_enabled(bool enabled) {
    search_button_->setEnabled(enabled);
}

void SearchWidget::set_results(std::vector<SearchResult> results) {
    clear_results();
    result_model_->set_results(std::move(results));
}

void SearchWidget::clear_results() {
    results_view_->clearSelection();
    result_model_->clear();

    selected_result_text_->clear();
    view_source_button_->setEnabled(false);

    emit selected_chunk_changed(QString());
}

void SearchWidget::reset() {
    query_edit_->clear();
    result_count_spin_->setValue(3);

    clear_results();
    set_search_enabled(false);

}

void SearchWidget::request_search() {
    if (!search_button_->isEnabled() || !has_query()) {
        return;
    }

    emit search_requested(query(), result_count());
}

void SearchWidget::show_selected_result() {
    QModelIndex index = results_view_->currentIndex();

    if (!index.isValid() || !results_view_->selectionModel()->hasSelection()) {
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    const SearchResult* result = result_model_->result_at(index.row());

    if (result == nullptr) {
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    QString details =
        "Document: " +
        QString::fromStdString(result->document_id) +
        "\nChunk: " +
        QString::number(result->chunk_sequence) +
        "\nScore: " +
        QString::number(result->score) +
        "\nMatched Terms: " +
        QString::number(result->matched_terms) +
        "\nText:\n" +
        QString::fromStdString(result->text);

    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true);

    emit selected_chunk_changed(
        QString::fromStdString(result->chunk_id));
}

void SearchWidget::request_source_document() {
    QModelIndex index = results_view_->currentIndex();

    if (!index.isValid() ||
        !results_view_->selectionModel()->hasSelection()) {
        return;
    }

    const SearchResult* result =
        result_model_->result_at(index.row());

    if (result == nullptr) {
        return;
    }

    emit source_document_requested(QString::fromStdString(result->document_id));
}

}  // namespace aiws::gui
