#include "ui/RecipeDetailsWidget.h"
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString text(const std::string &value) { return QString::fromUtf8(value.c_str()); }
} // namespace
RecipeDetailsWidget::RecipeDetailsWidget(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll);
    auto *body = new QWidget(scroll);
    body->setObjectName("pageBody");
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(30, 24, 30, 24);
    m_heading = new QLabel(body);
    m_heading->setObjectName(QStringLiteral("recipeTitle"));
    layout->addWidget(m_heading);
    m_summary = new QLabel(body);
    m_summary->setWordWrap(true);
    m_summary->setObjectName(QStringLiteral("recipeSummary"));
    layout->addWidget(m_summary);
    layout->addWidget(new QLabel(QStringLiteral("食材 · 1 份"), body));
    m_ingredients = new QVBoxLayout;
    layout->addLayout(m_ingredients);
    layout->addWidget(new QLabel(QStringLiteral("做法"), body));
    m_steps = new QVBoxLayout;
    layout->addLayout(m_steps);
    auto *complete = new QPushButton(QStringLiteral("开始烹饪"), body);
    complete->setObjectName(QStringLiteral("recipeComplete"));
    layout->addWidget(complete);
    connect(complete, &QPushButton::clicked, this, [this, complete] {
        if (!complete->property("started").toBool()) {
            complete->setProperty("started", true);
            complete->setText(QStringLiteral("我已做完这道菜"));
            return;
        }
        if (m_recipeId > 0)
            emit cookingRequested(m_recipeId);
    });
    layout->addStretch();
    setStyleSheet(QStringLiteral(
        "QWidget { color:#24455c; background:#f1f9fc; } QLabel#recipeTitle { font-size:25px; "
        "font-weight:700; } QLabel#recipeSummary { color:#527992; } QPushButton#recipeComplete { "
        "background:#358fca; color:white; padding:12px; border-radius:9px; }"));
}
void RecipeDetailsWidget::setRecipe(
    const Recipe &recipe, const NutritionInfo &nutrition,
    const std::vector<Shixu::Contracts::V1::IngredientAvailability> &availability) {
    using namespace Shixu::Contracts::V1;
    m_recipeId = recipe.id;
    findChild<QPushButton *>("recipeComplete")->setProperty("started", false);
    findChild<QPushButton *>("recipeComplete")->setText(QStringLiteral("开始烹饪"));
    m_heading->setText(text(recipe.name));
    m_summary->setText(QStringLiteral("约 %1 分钟 · 1 份 · %2 千卡 · 蛋白质 %3 克")
                           .arg(recipe.cookingTimeMinutes)
                           .arg(nutrition.calories, 0, 'f', 0)
                           .arg(nutrition.protein, 0, 'f', 1));
    auto clear = [](QVBoxLayout *layout) {
        while (auto *item = layout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    };
    clear(m_ingredients);
    clear(m_steps);
    for (const auto &ingredient : recipe.ingredients) {
        QString state = QStringLiteral("待核对");
        for (const auto &item : availability)
            if (item.foodName == ingredient.foodName) {
                state = item.state == AvailabilityState::Enough ? QStringLiteral("库存足够")
                        : item.state == AvailabilityState::Insufficient
                            ? QStringLiteral("约缺 %1 克").arg(item.knownGapGrams, 0, 'f', 0)
                            : QStringLiteral("库存重量未知");
                break;
            }
        auto *label = new QLabel(QStringLiteral("%1 · %2 克 · %3")
                                     .arg(text(ingredient.foodName))
                                     .arg(ingredient.amountGrams, 0, 'f', 0)
                                     .arg(state),
                                 this);
        m_ingredients->addWidget(label);
    }
    for (size_t i = 0; i < recipe.steps.size(); ++i) {
        auto *label = new QLabel(QStringLiteral("%1. %2").arg(i + 1).arg(text(recipe.steps[i])), this);
        label->setWordWrap(true);
        m_steps->addWidget(label);
    }
}

void RecipeDetailsWidget::showSteps() {
    if (m_steps->count() && m_steps->itemAt(0)->widget())
        findChild<QScrollArea *>()->ensureWidgetVisible(m_steps->itemAt(0)->widget());
}
