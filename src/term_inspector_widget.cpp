#include "aiws/gui/term_inspector_widget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>


namespace aiws::gui {

TermInspectorWidget::TermInspectorWidget(QWidget* parent) : QWidget(parent) {
    term_edit_ = new QLineEdit(this);
    term_edit_->setPlaceholderText("Enter term");

    inspect_button_ = new QPushButton("Inspect", this);

    document_frequency_value_ = new QLabel("-", this);
    chunk_frequency_value_ = new QLabel("-", this);

    auto* main_layout = new QVBoxLayout(this);

    auto* title_label = new QLabel("Term Inspector", this);
    main_layout->addWidget(title_label);

    auto* input_layout = new QHBoxLayout();
    input_layout->addWidget(new QLabel("Term:", this));
    input_layout->addWidget(term_edit_);
    input_layout->addWidget(inspect_button_);

    main_layout->addLayout(input_layout);

    auto* document_layout = new QHBoxLayout();
    document_layout->addWidget(new QLabel("Document Frequency:", this));
    document_layout->addWidget(document_frequency_value_);
    document_layout->addStretch();

    main_layout->addLayout(document_layout);

    auto* chunk_layout = new QHBoxLayout();
    chunk_layout->addWidget(new QLabel("Selected Chunk TF:", this));
    chunk_layout->addWidget(chunk_frequency_value_);
    chunk_layout->addStretch();

    main_layout->addLayout(chunk_layout);
    main_layout->addStretch();

    connect(inspect_button_, &QPushButton::clicked,this, &TermInspectorWidget::request_inspection);

    reset();
}

void TermInspectorWidget::set_inspection_enabled(bool enabled) {
    inspect_button_->setEnabled(enabled);

}

void TermInspectorWidget::set_selected_chunk(const QString& chunk_id) {
    selected_chunk_id_ = chunk_id;
}

void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency) {
     document_frequency_value_->setText(QString::number(document_frequency));

    if (has_chunk_frequency) {
        chunk_frequency_value_->setText(
            QString::number(chunk_frequency));
    } else {
        chunk_frequency_value_->setText("-");
    }
}

void TermInspectorWidget::clear_statistics() {
    document_frequency_value_->setText("-");
    chunk_frequency_value_->setText("-");
}

void TermInspectorWidget::reset() {
    term_edit_->clear();
    selected_chunk_id_.clear();

    clear_statistics();
    set_inspection_enabled(false);
}

void TermInspectorWidget::request_inspection() {
    if (!inspect_button_->isEnabled()) {
        return;
    }

    QString term = term_edit_->text().trimmed();

    if (term.isEmpty()) {
        return;
    }

    emit term_inspection_requested(term, selected_chunk_id_);
}

}  // namespace aiws::gui
