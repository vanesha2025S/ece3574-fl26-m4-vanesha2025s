#include "aiws/gui/context_widget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QString>
namespace aiws::gui {

ContextWidget::ContextWidget(QWidget* parent) : QWidget(parent) {
    context_budget_slider_ = new QSlider(Qt::Horizontal, this);
    context_budget_slider_->setRange(50, 1000);
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);
    context_budget_slider_->setTickInterval(50);
    context_budget_slider_->setValue(300);

    budget_value_label_ = new QLabel("300", this);

    build_context_button_ = new QPushButton("Build Context", this);

    context_text_ = new QPlainTextEdit(this);
    context_text_->setReadOnly(true);
    context_text_->setPlaceholderText("Build context to view results.");

    auto* main_layout = new QVBoxLayout(this);

    auto* title_label = new QLabel("Context", this);
    main_layout->addWidget(title_label);

    auto* budget_layout = new QHBoxLayout();
    budget_layout->addWidget(new QLabel("Token Budget:", this));
    budget_layout->addWidget(context_budget_slider_, 1);
    budget_layout->addWidget(budget_value_label_);

    main_layout->addLayout(budget_layout);
    main_layout->addWidget(build_context_button_);
    main_layout->addWidget(context_text_, 1);

    connect(context_budget_slider_, &QSlider::valueChanged,this, &ContextWidget::budget_changed);

    connect(build_context_button_, &QPushButton::clicked,this, &ContextWidget::request_context);

    reset();
}

int ContextWidget::token_budget() const {
    return context_budget_slider_->value();
}

void ContextWidget::set_build_context_enabled(bool enabled) {
    build_context_button_->setEnabled(enabled);
}

void ContextWidget::set_context(const std::vector<ContextItem>& items) {
    QString output;

    for (const ContextItem& item : items) {
        output += "Document: " +
            QString::fromStdString(item.document_id) + "\n";

        output += "Chunk: " +
            QString::number(item.chunk_sequence) + "\n";

        output += "Tokens: " +
            QString::number(item.token_count) + "\n";

        output += "Score: " +
            QString::number(item.score) + "\n";

        output += "Truncated: ";
        output += item.truncated ? "Yes\n" : "No\n";

        output += "Text:\n" +
            QString::fromStdString(item.text) + "\n";

        output += "\n------------------------\n\n";
    }

    if (items.empty()) {
        output = "No context items found.";
    }

    context_text_->setPlainText(output);
}

void ContextWidget::clear_context() {
    context_text_->clear();
}

void ContextWidget::reset() {
    context_budget_slider_->setValue(300);
    budget_value_label_->setText("300");

    clear_context();
    set_build_context_enabled(false);
}

void ContextWidget::request_context() {
     if (!build_context_button_->isEnabled()) {
        return;
    }

    emit build_context_requested(token_budget());
}

void ContextWidget::budget_changed(int value) {
    budget_value_label_->setText(QString::number(value));
}

}  // namespace aiws::gui
