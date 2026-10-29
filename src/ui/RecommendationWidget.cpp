#include "ui/RecommendationWidget.h"
#include "core/AppClock.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMetaMethod>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

using Mood = Shixu::RecommendationEngine::Mood;

// Map only for the old MainWindow route; V1 service uses the five-choice enum directly.
Mood legacyMood(Shixu::Contracts::V1::Mood mood) {
    using New = Shixu::Contracts::V1::Mood;
    switch (mood) {
    case New::Tired:
        return Mood::Tired;
    case New::WantGoodFood:
    case New::TreatMyself:
        return Mood::ReadyToCook;
    case New::LowAppetite:
    case New::WantLight:
        return Mood::Neutral;
    default:
        return Mood::NotSelected;
    }
}

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setTextFormat(Qt::PlainText);
    return label;
}

void addScoreRow(QVBoxLayout *layout, QWidget *parent, const QString &title, double score) {
    auto *row = new QHBoxLayout;
    row->setSpacing(12);

    auto *name = makeLabel(title, QStringLiteral("scoreName"), parent);
    name->setFixedWidth(72);
    row->addWidget(name);

    auto *bar = new QProgressBar(parent);
    bar->setObjectName(QStringLiteral("scoreBar"));
    bar->setRange(0, 100);
    bar->setValue(static_cast<int>(std::lround(std::clamp(score, 0.0, 100.0))));
    bar->setTextVisible(false);
    row->addWidget(bar, 1);

    auto *value = makeLabel(QStringLiteral("%1").arg(score, 0, 'f', 1), QStringLiteral("scoreValue"), parent);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setFixedWidth(44);
    row->addWidget(value);
    layout->addLayout(row);
}

QWidget *makeResultCard(const RecommendationViewItem &item, int rank, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("recommendationCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);

    auto *heading = new QHBoxLayout;
    heading->setSpacing(10);
    auto *rankLabel = makeLabel(QString::number(rank), QStringLiteral("rankBadge"), card);
    rankLabel->setAlignment(Qt::AlignCenter);
    rankLabel->setFixedSize(30, 30);
    heading->addWidget(rankLabel);

    auto *name = makeLabel(item.dishName, QStringLiteral("dishName"), card);
    name->setWordWrap(true);
    heading->addWidget(name, 1);

    auto *total = makeLabel(QStringLiteral("%1 分").arg(item.totalScore, 0, 'f', 1),
                            QStringLiteral("totalScore"), card);
    heading->addWidget(total);
    layout->addLayout(heading);

    auto *timeInfo = makeLabel(QStringLiteral("今天的心情：%1  ·  最多 %2 分钟  ·  这道菜约 %3 分钟")
                                   .arg(item.moodName)
                                   .arg(item.effectiveMaxPreparationMinutes)
                                   .arg(item.cookingTimeMinutes),
                               QStringLiteral("timeInfo"), card);
    timeInfo->setWordWrap(true);
    layout->addWidget(timeInfo);

    auto *breakdown = makeLabel(QStringLiteral("推荐依据"), QStringLiteral("recommendationSection"), card);
    layout->addWidget(breakdown);
    addScoreRow(layout, card, QStringLiteral("食材匹配"), item.ingredientMatchScore);
    addScoreRow(layout, card, QStringLiteral("临期利用"), item.expiryUrgencyScore);
    addScoreRow(layout, card, QStringLiteral("心情匹配"), item.moodMatchScore);
    if (item.nutritionScoreAvailable) {
        addScoreRow(layout, card, QStringLiteral("能量目标"), item.nutritionScore);
    }
    addScoreRow(layout, card, QStringLiteral("制作时间"), item.preparationTimeScore);

    auto *timeReference = makeLabel(
        QStringLiteral("制作时间得分按 %1 分钟基准计算。").arg(item.preparationTimeReferenceMinutes),
        QStringLiteral("timeReference"), card);
    layout->addWidget(timeReference);

    if (!item.missingIngredients.isEmpty()) {
        auto *missing =
            makeLabel(QStringLiteral("还缺食材：%1").arg(item.missingIngredients.join(QStringLiteral("、"))),
                      QStringLiteral("missingIngredients"), card);
        missing->setWordWrap(true);
        layout->addWidget(missing);
    }

    auto *reasonsTitle = makeLabel(QStringLiteral("推荐理由"), QStringLiteral("recommendationSection"), card);
    layout->addWidget(reasonsTitle);
    if (item.reasons.isEmpty()) {
        layout->addWidget(
            makeLabel(QStringLiteral("暂无详细理由"), QStringLiteral("recommendationReason"), card));
    } else {
        for (const QString &text : item.reasons) {
            auto *reason =
                makeLabel(QStringLiteral("• %1").arg(text), QStringLiteral("recommendationReason"), card);
            reason->setWordWrap(true);
            layout->addWidget(reason);
        }
    }
    return card;
}

} // namespace

RecommendationWidget::RecommendationWidget(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *body = new QWidget(scroll);
    body->setObjectName("pageBody");
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 24, 24, 24);
    bodyLayout->setSpacing(16);
    scroll->setWidget(body);

    auto *controls = new QFrame(body);
    controls->setObjectName(QStringLiteral("recommendationControls"));
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(22, 20, 22, 20);
    controlsLayout->setSpacing(12);
    controlsLayout->addWidget(
        makeLabel(QStringLiteral("今天想怎么做饭？"), QStringLiteral("moodHeading"), controls));
    controlsLayout->addWidget(makeLabel(QStringLiteral("先选今天的心情，再按需要调整制作时间和方向。"),
                                        QStringLiteral("controlsDescription"), controls));

    m_moodButtons = new QButtonGroup(this);
    m_moodButtons->setExclusive(true);
    auto *moodRow = new QHBoxLayout;
    moodRow->setSpacing(10);
    using VMood = Shixu::Contracts::V1::Mood;
    const struct {
        VMood mood;
        const char *name;
        int minutes;
    } choices[] = {
        {VMood::Tired, "很累", 30},
        {VMood::WantGoodFood, "想吃点好的", 45},
        {VMood::LowAppetite, "没胃口", 30},
        {VMood::WantLight, "想清爽一点", 40},
        {VMood::TreatMyself, "想犒劳自己", 75},
    };
    for (const auto &choice : choices) {
        auto *button = new QPushButton(QStringLiteral("%1").arg(QString::fromUtf8(choice.name)), controls);
        button->setObjectName(QStringLiteral("moodChoice"));
        button->setCheckable(true);
        button->setMinimumHeight(38);
        m_moodButtons->addButton(button, static_cast<int>(choice.mood));
        moodRow->addWidget(button, 1);
        connect(button, &QPushButton::clicked, this, [this, choice] {
            selectMoodV1(choice.mood);
            if (!isSignalConnected(QMetaMethod::fromSignal(&RecommendationWidget::moodSelectedV1)))
                emit moodSelected(legacyMood(choice.mood));
            emit moodSelectedV1(choice.mood);
            sendV1Request();
        });
    }
    controlsLayout->addLayout(moodRow);

    auto *limitRow = new QHBoxLayout;
    limitRow->addWidget(makeLabel(QStringLiteral("最多愿意做多久"), QStringLiteral("limitLabel"), controls));
    m_customLimit = new QSpinBox(controls);
    m_customLimit->setObjectName(QStringLiteral("customTimeLimit"));
    m_customLimit->setRange(0, 240);
    m_customLimit->setSpecialValueText(QStringLiteral("请先选择心情"));
    m_customLimit->setSuffix(QStringLiteral(" 分钟"));
    m_customLimit->setValue(0);
    limitRow->addWidget(m_customLimit);
    limitRow->addStretch();
    controlsLayout->addLayout(limitRow);

    m_quickLayout = new QVBoxLayout;

    m_policyHint = makeLabel(QStringLiteral("库存方式：可补购"), QStringLiteral("limitHelp"), controls);
    controlsLayout->addWidget(m_policyHint);
    connect(m_customLimit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        resetExcludedRecipeIds();
        if (m_moodV1 != VMood::NotSelected)
            sendV1Request();
    });

    m_selectionHint =
        makeLabel(QStringLiteral("请先选择今天的心情。"), QStringLiteral("selectionHint"), controls);
    m_selectionHint->hide();
    controlsLayout->addWidget(m_selectionHint);

    auto *generateButton = new QPushButton(QStringLiteral("生成推荐"), controls);
    generateButton->setObjectName(QStringLiteral("generateRecommendations"));
    generateButton->setCursor(Qt::PointingHandCursor);
    generateButton->setMinimumHeight(42);
    generateButton->hide();
    connect(generateButton, &QPushButton::clicked, this, &RecommendationWidget::requestRecommendations);
    bodyLayout->addWidget(controls);

    m_warning = makeLabel({}, QStringLiteral("recommendationWarnings"), body);
    m_warning->setWordWrap(true);
    m_warning->hide();
    bodyLayout->addWidget(m_warning);

    m_summary = makeLabel({}, QStringLiteral("recommendationSummary"), body);
    m_summary->hide();
    bodyLayout->addWidget(m_summary);

    m_emptyState = new QWidget(body);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(10);
    m_emptyTitle = makeLabel(QStringLiteral("还没有生成推荐"), QStringLiteral("emptyTitle"), m_emptyState);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyDescription = makeLabel(QStringLiteral("选择今天的心情，我来帮你挑。"),
                                   QStringLiteral("emptyDescription"), m_emptyState);
    m_emptyDescription->setAlignment(Qt::AlignCenter);
    m_emptyDescription->setWordWrap(true);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyDescription);
    bodyLayout->addWidget(m_emptyState, 1);

    auto *results = new QWidget(body);
    m_resultsLayout = new QVBoxLayout(results);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(14);
    bodyLayout->addWidget(results);
    m_swapButton = new QPushButton(QStringLiteral("换一批"), body);
    m_swapButton->setObjectName(QStringLiteral("swapRecommendations"));
    m_swapButton->hide();
    bodyLayout->addWidget(m_swapButton);
    connect(m_swapButton, &QPushButton::clicked, this, [this] {
        m_excludedIds.insert(m_excludedIds.end(), m_visibleIds.begin(), m_visibleIds.end());
        m_visibleIds.clear();
        sendV1Request();
    });
    bodyLayout->addWidget(makeLabel(QStringLiteral("这些还不太想吃？"), "controlsTitle", body));
    bodyLayout->addLayout(m_quickLayout);
    auto *reset = new QPushButton(QStringLiteral("恢复默认"), body);
    bodyLayout->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, [this] {
        selectMoodV1(m_moodV1);
        sendV1Request();
    });
    auto *policy = new QComboBox(controls);
    policy->setObjectName("inventoryPolicy");
    policy->addItems(
        {QStringLiteral("优先现有，允许补购"), QStringLiteral("只用现有食材"), QStringLiteral("少量补购")});
    limitRow->addWidget(policy);
    connect(policy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        m_policy = static_cast<Shixu::Contracts::V1::InventoryPolicy>(i);
        resetExcludedRecipeIds();
        sendV1Request();
    });
    for (int minutes : {20, 30, 45}) {
        auto *time = new QPushButton(QString::number(minutes), controls);
        limitRow->insertWidget(1, time);
        connect(time, &QPushButton::clicked, this, [this, minutes] { m_customLimit->setValue(minutes); });
    }
    bodyLayout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QFrame#recommendationControls, QFrame#recommendationCard {
            background: #F9FCFD; border: 1px solid #DCEAF0; border-radius: 12px;
        }
        QLabel#controlsTitle, QLabel#moodHeading { color: #244E6A; font-size: 21px; font-weight: 700; }
        QLabel#controlsDescription, QLabel#limitHelp, QLabel#timeReference {
            color: #6B8797; font-size: 13px;
        }
        QLabel#limitLabel { color: #31566D; font-size: 14px; }
        QPushButton#moodChoice {
            text-align: center; color: #31566D; background: #F0F7FA;
            border: 1px solid #D6E7EF; border-radius: 10px; padding: 8px;
            font-size: 14px;
        }
        QPushButton#moodChoice:hover { background: #E8F3F8; }
        QPushButton#moodChoice:checked {
            color: #185D88; background: #DCEEF8; border: 2px solid #62A9D3;
            font-weight: 700;
        }
        QPushButton#generateRecommendations {
            color: white; background: #358FCA; border: 0; border-radius: 9px;
            font-size: 15px; font-weight: 700;
        }
        QPushButton#generateRecommendations:hover { background: #267DB6; }
        QLabel#selectionHint { color: #C05239; font-size: 13px; }
        QLabel#recommendationWarnings {
            color: #805926; background: #FFF6E6; border: 1px solid #F0DDB4;
            border-radius: 8px; padding: 12px; font-size: 13px;
        }
        QLabel#recommendationSummary { color: #607F91; font-size: 14px; }
        QLabel#emptyTitle { color: #31566D; font-size: 22px; font-weight: 600; }
        QLabel#emptyDescription { color: #7893A0; font-size: 14px; }
        QLabel#rankBadge {
            background: #DCEEF8; color: #1B6297; border-radius: 15px; font-weight: 700;
        }
        QLabel#dishName { color: #26465B; font-size: 19px; font-weight: 600; }
        QLabel#totalScore { color: #1B6297; font-size: 18px; font-weight: 700; }
        QLabel#timeInfo { color: #2B719D; font-size: 14px; font-weight: 600; }
        QLabel#recommendationSection { color: #435F70; font-size: 14px; font-weight: 600; }
        QLabel#scoreName, QLabel#recommendationReason, QLabel#missingIngredients {
            color: #536F7F; font-size: 13px;
        }
        QLabel#scoreValue { color: #1B6297; font-size: 13px; }
        QProgressBar#scoreBar {
            background: #E6F0F4; border: 0; border-radius: 4px; max-height: 8px;
        }
        QProgressBar#scoreBar::chunk { background: #63A9D2; border-radius: 4px; }
    )"));
}

void RecommendationWidget::selectMood(Shixu::RecommendationEngine::Mood mood) {
    using Old = Shixu::RecommendationEngine::Mood;
    using New = Shixu::Contracts::V1::Mood;
    switch (mood) {
    case Old::ReadyToCook:
        selectMoodV1(New::WantGoodFood);
        break;
    case Old::Neutral:
        selectMoodV1(New::LowAppetite);
        break;
    case Old::Tired:
        selectMoodV1(New::Tired);
        break;
    default:
        break;
    }
}

void RecommendationWidget::requestRecommendations() { sendV1Request(); }

void RecommendationWidget::setWarnings(const QStringList &warnings) {
    m_warning->setVisible(!warnings.isEmpty());
    m_warning->setText(warnings.isEmpty()
                           ? QString()
                           : QStringLiteral("库存提示：\n• %1").arg(warnings.join(QStringLiteral("\n• "))));
}

void RecommendationWidget::setRecommendations(const QVector<RecommendationViewItem> &items) {
    while (QLayoutItem *child = m_resultsLayout->takeAt(0)) {
        if (QWidget *widget = child->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete child;
    }

    m_emptyState->setVisible(items.isEmpty());
    m_summary->setVisible(!items.isEmpty());
    if (items.isEmpty()) {
        m_emptyTitle->setText(QStringLiteral("没有符合条件的菜谱"));
        m_emptyDescription->setText(QStringLiteral("试试调整心情或最多愿意做多久，也可以先录入更多食材。"));
        return;
    }

    m_summary->setText(QStringLiteral("为你找到 %1 道推荐菜谱").arg(items.size()));
    for (int index = 0; index < items.size(); ++index) {
        m_resultsLayout->addWidget(makeResultCard(items.at(index), index + 1, this));
    }
}

void RecommendationWidget::resetExcludedRecipeIds() {
    m_excludedIds.clear();
    m_visibleIds.clear();
    if (m_swapButton)
        m_swapButton->hide();
}

void RecommendationWidget::selectMoodV1(Shixu::Contracts::V1::Mood mood) {
    namespace V = Shixu::Contracts::V1;
    int minutes = 0;
    switch (mood) {
    case V::Mood::Tired:
        minutes = 30;
        break;
    case V::Mood::WantGoodFood:
        minutes = 45;
        break;
    case V::Mood::LowAppetite:
        minutes = 30;
        break;
    case V::Mood::WantLight:
        minutes = 40;
        break;
    case V::Mood::TreatMyself:
        minutes = 75;
        break;
    default:
        break;
    }
    if (minutes == 0)
        return;
    const QStringList titles = {QStringLiteral("今天很累"), QStringLiteral("今天想吃点好的"),
                                QStringLiteral("今天没什么胃口"), QStringLiteral("今天想清爽一点"),
                                QStringLiteral("今天想犒劳自己")};
    const QStringList descriptions = {
        QStringLiteral("晚饭简单一点，我帮你挑。"), QStringLiteral("好好吃一顿，从这些菜里选。"),
        QStringLiteral("挑点开胃的，轻松吃一餐。"), QStringLiteral("清淡有滋味，吃得舒服一点。"),
        QStringLiteral("给今天加一道特别的菜。")};
    findChild<QLabel *>("moodHeading")->setText(titles.at(int(mood) - 1));
    findChild<QLabel *>("controlsDescription")->setText(descriptions.at(int(mood) - 1));
    m_moodV1 = mood;
    QSignalBlocker timeBlocker(m_customLimit);
    m_refinement = V::Refinement::None;
    m_refinements.clear();
    m_policy = m_defaultPolicy;
    if (auto *policy = findChild<QComboBox *>("inventoryPolicy")) {
        QSignalBlocker b(policy);
        policy->setCurrentIndex(int(m_policy));
    }
    m_policyHint->setText(QStringLiteral("调整条件可叠加，选中项再次点击可撤销。"));
    m_customLimit->setValue(minutes);
    resetExcludedRecipeIds();
    if (auto *button = qobject_cast<QPushButton *>(m_moodButtons->button(static_cast<int>(mood))))
        button->setChecked(true);
    m_selectionHint->hide();
    updateQuickButtons();
}

void RecommendationWidget::updateQuickButtons() {
    namespace V = Shixu::Contracts::V1;
    while (auto *item = m_quickLayout->takeAt(0)) {
        if (auto *widget = item->widget())
            delete widget;
        delete item;
    }
    struct Quick {
        const char *label;
        V::Refinement refinement;
        V::InventoryPolicy policy;
        bool shortTime;
    };
    const Quick tired[] = {{"更省事一点", V::Refinement::Simpler, V::InventoryPolicy::PreferInventory, false},
                           {"不想吃面", V::Refinement::NoNoodles, V::InventoryPolicy::PreferInventory, false},
                           {"只用冰箱里的", V::Refinement::None, V::InventoryPolicy::OnlyInventory, false}};
    const Quick good[] = {{"更丰盛", V::Refinement::Heartier, V::InventoryPolicy::PreferInventory, false},
                          {"清淡一点", V::Refinement::Lighter, V::InventoryPolicy::PreferInventory, false},
                          {"只用冰箱里的", V::Refinement::None, V::InventoryPolicy::OnlyInventory, false}};
    const Quick appetite[] = {
        {"更开胃", V::Refinement::MoreAppetizing, V::InventoryPolicy::PreferInventory, false},
        {"清淡一点", V::Refinement::Lighter, V::InventoryPolicy::PreferInventory, false},
        {"更省事一点", V::Refinement::Simpler, V::InventoryPolicy::PreferInventory, false}};
    const Quick light[] = {
        {"多点蔬菜", V::Refinement::MoreVegetables, V::InventoryPolicy::PreferInventory, false},
        {"多点蛋白质", V::Refinement::MoreProtein, V::InventoryPolicy::PreferInventory, false},
        {"更省事一点", V::Refinement::Simpler, V::InventoryPolicy::PreferInventory, false}};
    const Quick treat[] = {
        {"更有特色", V::Refinement::MoreSpecial, V::InventoryPolicy::PreferInventory, false},
        {"少量补购", V::Refinement::None, V::InventoryPolicy::SmallPurchase, false},
        {"时间短一点", V::Refinement::None, V::InventoryPolicy::PreferInventory, true}};
    const Quick *choices = nullptr;
    switch (m_moodV1) {
    case V::Mood::Tired:
        choices = tired;
        break;
    case V::Mood::WantGoodFood:
        choices = good;
        break;
    case V::Mood::LowAppetite:
        choices = appetite;
        break;
    case V::Mood::WantLight:
        choices = light;
        break;
    case V::Mood::TreatMyself:
        choices = treat;
        break;
    default:
        return;
    }
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        const Quick quick = choices[i];
        auto *button = new QPushButton(QString::fromUtf8(quick.label), row);
        button->setObjectName(QStringLiteral("moodChoice"));
        button->setCheckable(!quick.shortTime);
        button->setChecked(quick.refinement != V::Refinement::None
                               ? m_refinements.count(quick.refinement)
                               : (!quick.shortTime && m_policy == quick.policy &&
                                  quick.policy != V::InventoryPolicy::PreferInventory));
        if (button->isChecked())
            button->setText(button->text() + QStringLiteral(" ×"));
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, quick] {
            if (quick.shortTime) {
                if (m_customLimit->value() > 45)
                    m_customLimit->setValue(45);
                else
                    m_selectionHint->setText(QStringLiteral("当前已在 45 分钟内。"));
            } else if (quick.refinement != V::Refinement::None) {
                if (!m_refinements.erase(quick.refinement))
                    m_refinements.insert(quick.refinement);
            } else {
                m_policy = m_policy == quick.policy ? V::InventoryPolicy::PreferInventory : quick.policy;
            }
            if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
                QSignalBlocker b(combo);
                combo->setCurrentIndex(int(m_policy));
            }
            m_policyHint->setText(
                m_policy == V::InventoryPolicy::OnlyInventory   ? QStringLiteral("库存方式：只用冰箱里的")
                : m_policy == V::InventoryPolicy::SmallPurchase ? QStringLiteral("库存方式：少量补购")
                                                                : QStringLiteral("库存方式：可补购"));
            resetExcludedRecipeIds();
            updateQuickButtons();
            sendV1Request();
        });
    }
    m_quickLayout->addWidget(row);
}

void RecommendationWidget::sendV1Request() {
    namespace V = Shixu::Contracts::V1;
    if (m_moodV1 == V::Mood::NotSelected || m_customLimit->value() <= 0) {
        m_selectionHint->setText(QStringLiteral("请先选择今天的心情。"));
        m_selectionHint->show();
        return;
    }
    m_selectionHint->hide();
    V::RecommendationPreferences preferences;
    preferences.mood = m_moodV1;
    preferences.maxPreparationMinutes = m_customLimit->value();
    preferences.inventoryPolicy = m_policy;
    preferences.refinements.assign(m_refinements.begin(), m_refinements.end());
    preferences.excludedRecipeIds = m_excludedIds;
    preferences.servings = 1;
    preferences.referenceDate = Shixu::appNow().date().toString(Qt::ISODate).toStdString();
    if (!isSignalConnected(QMetaMethod::fromSignal(&RecommendationWidget::recommendationRequestedV1)))
        emit recommendationRequested(legacyMood(m_moodV1), preferences.maxPreparationMinutes);
    emit recommendationRequestedV1(preferences);
}

void RecommendationWidget::setRecommendationResult(const Shixu::Contracts::V1::RecommendationResult &result) {
    namespace V = Shixu::Contracts::V1;
    while (auto *item = m_resultsLayout->takeAt(0)) {
        if (auto *widget = item->widget())
            delete widget;
        delete item;
    }
    m_visibleIds.clear();
    m_swapButton->setVisible(result.outcome.status == V::OperationStatus::Succeeded && !result.cards.empty());
    m_swapButton->setEnabled(result.hasMore);
    m_swapButton->setText(result.hasMore ? QStringLiteral("换一批") : QStringLiteral("已是最后一批"));
    const bool failed = result.outcome.status != V::OperationStatus::Succeeded;
    m_emptyState->setVisible(failed || result.cards.empty());
    m_summary->setVisible(!failed && !result.cards.empty());
    if (failed) {
        m_emptyTitle->setText(QStringLiteral("推荐暂时不可用"));
        m_emptyDescription->setText(QString::fromUtf8(result.outcome.message.c_str()));
        return;
    }
    if (result.cards.empty()) {
        m_emptyTitle->setText(QStringLiteral("暂时没有符合条件的菜谱"));
        m_emptyDescription->setText(QString::fromUtf8(result.outcome.message.c_str()) +
                                    QStringLiteral(" · 可调整上方时间或库存方式，或恢复默认。"));
        return;
    }
    m_summary->setText(QStringLiteral("今晚推荐 · 还有 %1 道符合条件 · 本批 %2 道")
                           .arg(result.eligibleCount)
                           .arg(result.cards.size()));
    for (const auto &card : result.cards) {
        m_visibleIds.push_back(card.recipeId);
        const QString role = card.role == V::CardRole::Primary          ? QStringLiteral("今晚首选")
                             : card.role == V::CardRole::AlternativeOne ? QStringLiteral("备选一")
                                                                        : QStringLiteral("备选二");
        auto *frame = new QFrame(this);
        frame->setObjectName(QStringLiteral("recommendationCard"));
        auto *layout = new QVBoxLayout(frame);
        layout->addWidget(
            makeLabel(role + QStringLiteral(" · ") + card.dishName, QStringLiteral("dishName"), frame));
        const bool primary = card.role == V::CardRole::Primary;
        frame->setProperty("primaryCard", primary);
        if (primary)
            frame->setStyleSheet(
                "QFrame#recommendationCard {border:2px solid #79B6D8;background:#F5FBFF;border-radius:14px;} "
                "QLabel#dishName {font-size:24px;font-weight:700;}");
        layout->setContentsMargins(22, 18, 22, 18);
        layout->setSpacing(10);
        QStringList gaps;
        bool unknown = false;
        for (const auto &availability : card.availability) {
            if (availability.state == V::AvailabilityState::Enough)
                continue;
            unknown |= availability.state == V::AvailabilityState::Unknown;
            gaps << QStringLiteral("%1：已知数量还差 %2 克%3")
                        .arg(QString::fromUtf8(availability.foodName.c_str()))
                        .arg(availability.knownGapGrams, 0, 'f', 0)
                        .arg(availability.state == V::AvailabilityState::Unknown
                                 ? QStringLiteral("，另有数量待确认")
                                 : QString());
        }
        layout->addWidget(makeLabel(QStringLiteral("%1 分钟 · %2 步 · %3")
                                        .arg(card.recipe.cookingTimeMinutes)
                                        .arg(card.recipe.steps.size())
                                        .arg(gaps.isEmpty() ? QStringLiteral("食材齐全")
                                             : unknown      ? QStringLiteral("数量待确认")
                                                            : QStringLiteral("需要补充食材")),
                                    "timeInfo", frame));
        if (!result.cards.empty() && !primary &&
            card.recipe.cookingTimeMinutes < result.cards.front().recipe.cookingTimeMinutes)
            layout->addWidget(makeLabel(QStringLiteral("比首选更快完成"), "recommendationReason", frame));
        QString reasonText =
            gaps.isEmpty()
                ? QStringLiteral("家里食材齐全，可以直接开始。")
                : QStringLiteral("做法共 %1 步，所缺食材可展开查看。").arg(card.recipe.steps.size());
        if (card.expiryUrgencyScore > 0)
            reasonText += QStringLiteral(" 可优先利用临期食材。");
        auto *reason = makeLabel(reasonText, "recommendationReason", frame);
        reason->setWordWrap(true);
        layout->addWidget(reason);
        auto *more = new QPushButton(QStringLiteral("食材与推荐依据 ▾"), frame);
        more->setCheckable(true);
        layout->addWidget(more);
        auto *explanation =
            makeLabel(gaps.join("\n") + QStringLiteral("\n综合评分 %1\n").arg(card.totalScore, 0, 'f', 1) +
                          card.reasons.join("\n"),
                      "recommendationReason", frame);
        explanation->setWordWrap(true);
        explanation->hide();
        layout->addWidget(explanation);
        connect(more, &QPushButton::toggled, explanation, &QWidget::setVisible);
        auto *actions = new QHBoxLayout;
        auto *choose =
            new QPushButton(primary ? QStringLiteral("就做这道") : QStringLiteral("选这道"), frame);
        choose->setProperty("primary", true);
        actions->addWidget(choose);
        connect(choose, &QPushButton::clicked, this, [this, id = card.recipeId] { emit recipeSelected(id); });
        auto *detail = new QPushButton(QStringLiteral("查看菜谱详情"), frame);
        detail->setToolTip(QStringLiteral("查看做法"));
        actions->addWidget(detail);
        actions->addStretch();
        layout->addLayout(actions);
        connect(detail, &QPushButton::clicked, this,
                [this, id = card.recipeId] { emit recipeStepsSelected(id); });
        m_resultsLayout->addWidget(frame);
    }
}

void RecommendationWidget::setDefaultInventoryPolicy(Shixu::Contracts::V1::InventoryPolicy policy) {
    m_defaultPolicy = policy;
    m_policy = policy;
    if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
        QSignalBlocker b(combo);
        combo->setCurrentIndex(int(policy));
    }
    resetExcludedRecipeIds();
}
