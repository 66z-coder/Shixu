#include "ui/AppIcons.h"
#include "ui/RecipeDetailsWidget.h"
#include "core/PantryStaples.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace {
QString text(const std::string &value) { return QString::fromUtf8(value.c_str()); }
} // namespace
RecipeDetailsWidget::RecipeDetailsWidget(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    m_scroll = scroll;
    scroll->setObjectName("recipeDetailsScroll");
    scroll->setProperty("pageScroll", true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll);
    auto *body = new QWidget(scroll);
    body->setObjectName("pageBody");
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    m_heading = new QLabel(body);
    m_heading->setObjectName(QStringLiteral("recipeTitle"));
    m_heading->setWordWrap(true);
    auto *titleRow = new QHBoxLayout;
    m_categoryIcon = new QLabel(body);
    m_categoryIcon->setObjectName("recipeCategoryIcon");
    titleRow->setSpacing(12);
    titleRow->addWidget(m_categoryIcon, 0, Qt::AlignTop);
    titleRow->addWidget(m_heading, 1);
    layout->addLayout(titleRow);
    m_summary = new QLabel(body);
    m_summary->setWordWrap(true);
    m_summary->setObjectName(QStringLiteral("recipeSummary"));
    layout->addWidget(m_summary);
    auto *nutritionCard = new QFrame(body);
    m_nutritionCard = nutritionCard;
    nutritionCard->setObjectName(QStringLiteral("recipeSectionCard"));
    auto *nutritionBox = new QVBoxLayout(nutritionCard);
    nutritionBox->setContentsMargins(20, 20, 20, 20);
    nutritionBox->setSpacing(12);
    auto *nutritionTitle = new QLabel(QStringLiteral("每份营养 · 估算"), nutritionCard);
    nutritionTitle->setObjectName(QStringLiteral("recipeSection"));
    nutritionBox->addWidget(nutritionTitle);
    auto *calorieCard = new QFrame(nutritionCard);
    calorieCard->setObjectName(QStringLiteral("recipeCalorieCard"));
    auto *calorieLayout = new QVBoxLayout(calorieCard);
    calorieLayout->setContentsMargins(16, 14, 16, 14);
    calorieLayout->setSpacing(2);
    m_calories = new QLabel(calorieCard);
    m_calories->setObjectName(QStringLiteral("recipeCalories"));
    m_calories->setAlignment(Qt::AlignCenter);
    calorieLayout->addWidget(m_calories);
    auto *calorieCaption = new QLabel(QStringLiteral("千卡 / 份"), calorieCard);
    calorieCaption->setObjectName(QStringLiteral("recipeCalorieCaption"));
    calorieCaption->setAlignment(Qt::AlignCenter);
    calorieLayout->addWidget(calorieCaption);
    nutritionBox->addWidget(calorieCard);
    for (const auto &name : {QStringLiteral("蛋白质"), QStringLiteral("碳水化合物"), QStringLiteral("脂肪")}) {
        auto *row = new QHBoxLayout;
        auto *caption = new QLabel(name, nutritionCard);
        caption->setObjectName(QStringLiteral("recipeMacroCaption"));
        row->addWidget(caption);
        row->addStretch();
        auto *value = new QLabel(nutritionCard);
        value->setObjectName(QStringLiteral("recipeMacroValue%1").arg(m_macroValues.size()));
        m_macroValues.append(value);
        row->addWidget(value);
        nutritionBox->addLayout(row);
        auto *bar = new QProgressBar(nutritionCard);
        bar->setObjectName(QStringLiteral("recipeMacroBar%1").arg(m_macroBars.size()));
        bar->setRange(0, 1000);
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        m_macroBars.append(bar);
        nutritionBox->addWidget(bar);
    }
    auto *note = new QLabel(QStringLiteral("横条表示三大营养素的估算供能占比，并非摄入目标。"), nutritionCard);
    note->setObjectName(QStringLiteral("recipeNutritionNote"));
    note->setWordWrap(true);
    nutritionBox->addWidget(note);

    nutritionCard->hide();
    auto *ingredientCard = new QFrame(body);
    ingredientCard->setObjectName(QStringLiteral("recipeSectionCard"));
    auto *ingredientBox = new QVBoxLayout(ingredientCard);
    ingredientBox->setContentsMargins(20, 20, 20, 20);
    ingredientBox->setSpacing(8);
    auto *ingredientHeading = new QLabel(QStringLiteral("食材 · 1 份"), ingredientCard);
    ingredientHeading->setObjectName(QStringLiteral("recipeSection"));
    ingredientBox->addWidget(ingredientHeading);
    m_ingredients = new QVBoxLayout;
    m_ingredients->setSpacing(8);
    ingredientBox->addLayout(m_ingredients);
    layout->addWidget(ingredientCard);
    auto *stepsCard = new QFrame(body);
    stepsCard->setObjectName(QStringLiteral("recipeSectionCard"));
    auto *stepsBox = new QVBoxLayout(stepsCard);
    stepsBox->setContentsMargins(20, 20, 20, 20);
    stepsBox->setSpacing(8);
    auto *stepsHeading = new QLabel(QStringLiteral("做法"), stepsCard);
    stepsHeading->setObjectName(QStringLiteral("recipeSection"));
    stepsBox->addWidget(stepsHeading);
    m_steps = new QVBoxLayout;
    m_steps->setSpacing(10);
    stepsBox->addLayout(m_steps);
    layout->addWidget(stepsCard);
    layout->addWidget(nutritionCard);
    auto *complete = new QPushButton(QStringLiteral("完成做饭并打卡"), body);
    complete->setObjectName(QStringLiteral("recipeComplete"));
    complete->setProperty("primary", true);
    complete->setMinimumWidth(180);
    layout->addWidget(complete, 0, Qt::AlignLeft);
    complete->setEnabled(false);
    m_heading->setText(QStringLiteral("请选择菜谱"));
    m_summary->setText(QStringLiteral("选择左侧菜谱，查看营养和做法。"));
    connect(complete, &QPushButton::clicked, this, [this] {
        if (m_recipeId > 0)
            emit cookingRequested(m_recipeId);
    });
    layout->addStretch();
    setStyleSheet(QStringLiteral(
        "QWidget { color:#24455c; } QWidget#pageBody { background:#EDF7F9; } QLabel#recipeTitle { font-size:24px; "
        "font-weight:700; } QLabel#recipeSummary { color:#527992; } QFrame#recipeSectionCard { background:white; border:1px solid #DCE8EF; border-radius:14px; } QFrame#recipeSectionCard QLabel { background:transparent; } QLabel#recipeSection { font-size:18px; font-weight:700; } QFrame#recipeCalorieCard { background:#EFF7FB; border:0; border-radius:10px; } QLabel#recipeCalories { font-size:32px; font-weight:700; color:#287BA8; } QLabel#recipeCalorieCaption { color:#6A8798; font-size:14px; } QLabel#recipeMacroCaption { color:#315970; font-size:14px; } QLabel#recipeMacroValue0, QLabel#recipeMacroValue1, QLabel#recipeMacroValue2 { color:#244E6A; font-size:15px; font-weight:600; } QLabel#recipeNutritionNote { color:#718A9A; font-size:12px; } QProgressBar { background:#E8F0F4; border:0; border-radius:4px; } QProgressBar#recipeMacroBar0::chunk { background:#68A0B8; border-radius:4px; } QProgressBar#recipeMacroBar1::chunk { background:#C2A366; border-radius:4px; } QProgressBar#recipeMacroBar2::chunk { background:#8D89B9; border-radius:4px; } QLabel[needsAttention=\"true\"] { color:#9A4E2D; font-weight:600; }"));
}
void RecipeDetailsWidget::setRecipe(
    const Recipe &recipe, const NutritionInfo &nutrition,
    const std::vector<Shixu::Contracts::V1::IngredientAvailability> &availability) {
    using namespace Shixu::Contracts::V1;
    m_recipeId = recipe.id;
    findChild<QPushButton *>("recipeComplete")->setEnabled(recipe.id > 0);
    m_heading->setText(text(recipe.name));
    m_categoryIcon->setPixmap(Shixu::Ui::foodPixmap(text(recipe.category), 30, devicePixelRatioF()));
    m_categoryIcon->setToolTip(text(recipe.category));
    m_summary->setText(QStringLiteral("约 %1 分钟 · 1 份 · 约 %2 千卡 · 估算").arg(recipe.cookingTimeMinutes).arg(nutrition.calories, 0, 'f', 0));
    m_nutritionCard->show();
    m_calories->setText(QString::number(nutrition.calories, 'f', 0));
    const double amounts[] = {nutrition.protein, nutrition.carbohydrates, nutrition.fat};
    const double energy[] = {amounts[0] * 4, amounts[1] * 4, amounts[2] * 9};
    const double total = energy[0] + energy[1] + energy[2];
    for (int i = 0; i < 3; ++i) {
        m_macroValues[i]->setText(QStringLiteral("%1 克").arg(amounts[i], 0, 'f', 1));
        m_macroBars[i]->setValue(std::isfinite(total) && total > 0 && std::isfinite(energy[i])
                                     ? qRound(1000 * energy[i] / total) : 0);
    }
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
        bool needsAttention = true;
        for (const auto &item : availability)
            if (item.foodName == ingredient.foodName) {
                needsAttention = item.state != AvailabilityState::Enough;
                state = item.state == AvailabilityState::Enough ? QStringLiteral("库存足够")
                        : item.state == AvailabilityState::Insufficient
                            ? QStringLiteral("约缺 %1 克").arg(item.knownGapGrams, 0, 'f', 0)
                            : QStringLiteral("库存重量未知");
                break;
            }
        if (Shixu::isPantryStaple(ingredient.foodName)) {
            state = QStringLiteral("常备调料 · 默认已备好");
            needsAttention = false;
        }
        auto *label = new QLabel(QStringLiteral("%1 · %2 克 · %3")
                                     .arg(text(ingredient.foodName))
                                     .arg(ingredient.amountGrams, 0, 'f', 0)
                                     .arg(state),
                                 this);
        label->setWordWrap(true);
        label->setProperty("needsAttention", needsAttention);
        m_ingredients->addWidget(label);
    }
    for (size_t i = 0; i < recipe.steps.size(); ++i) {
        auto *row = new QWidget(this);
        auto *stepLayout = new QHBoxLayout(row);
        stepLayout->setContentsMargins(0, 4, 0, 4);
        stepLayout->setSpacing(12);
        auto *number = new QLabel(QString::number(i + 1), row);
        number->setAlignment(Qt::AlignCenter);
        number->setFixedSize(28, 28);
        number->setStyleSheet(QStringLiteral("background:#E8F3FA;color:#287BA8;border-radius:14px;font-weight:600;"));
        stepLayout->addWidget(number, 0, Qt::AlignTop);
        auto *label = new QLabel(text(recipe.steps[i]), row);
        label->setWordWrap(true);
        stepLayout->addWidget(label, 1);
        m_steps->addWidget(row);
    }
}

void RecipeDetailsWidget::clearRecipe() {
    m_recipeId = 0;
    m_categoryIcon->clear();
    findChild<QPushButton *>("recipeComplete")->setEnabled(false);
    m_heading->setText(QStringLiteral("无法加载菜谱"));
    m_summary->setText(QStringLiteral("请选择菜谱重试。"));
    m_nutritionCard->hide();
    m_calories->clear();
    for (auto *value : m_macroValues) value->clear();
    for (auto *bar : m_macroBars) bar->setValue(0);
    for (auto *layout : {m_ingredients, m_steps})
        while (auto *item = layout->takeAt(0)) { delete item->widget(); delete item; }
}

void RecipeDetailsWidget::showSteps() {
    if (m_steps->count() && m_steps->itemAt(0)->widget())
        findChild<QScrollArea *>()->ensureWidgetVisible(m_steps->itemAt(0)->widget());
}

void RecipeDetailsWidget::scrollToTop() {
    m_scroll->verticalScrollBar()->setValue(0);
    QTimer::singleShot(0, this, [this] { m_scroll->verticalScrollBar()->setValue(0); });
}
