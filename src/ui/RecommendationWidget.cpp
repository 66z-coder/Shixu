#include "ui/AppIcons.h"
#include "ui/RecommendationWidget.h"
#include "core/AppClock.h"
#include "core/PantryStaples.h"
#include "ui/RecipeDetailsWidget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMetaMethod>
#include <QDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

using Mood = Shixu::RecommendationEngine::Mood;

class BatchRepaint {
public:
    explicit BatchRepaint(QWidget *widget) : widget_(widget), wasEnabled_(widget->updatesEnabled()) {
        if (wasEnabled_) widget_->setUpdatesEnabled(false);
    }
    ~BatchRepaint() { if (wasEnabled_) widget_->setUpdatesEnabled(true); }
private:
    QWidget *widget_;
    bool wasEnabled_;
};

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
    outer->setSpacing(16);
    auto *stageRow = new QHBoxLayout;
    stageRow->setSpacing(8);
    for (const auto &name : {QStringLiteral("1  选择需求"), QStringLiteral("2  查看推荐"), QStringLiteral("3  菜谱详情")}) {
        auto *stage = makeLabel(name, QStringLiteral("recommendationStage"), this);
        stage->setAlignment(Qt::AlignCenter);
        m_stageLabels.append(stage);
        stageRow->addWidget(stage, 1);
    }
    outer->addLayout(stageRow);
    m_stages = new QStackedWidget(this);
    m_stages->setObjectName("recommendationStages");
    outer->addWidget(m_stages, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setProperty("pageScroll", true);
    scroll->setObjectName("recommendationSetupScroll");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_stages->addWidget(scroll);

    auto *body = new QWidget(scroll);
    body->setObjectName("pageBody");
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(16);
    scroll->setWidget(body);

    auto *controls = new QFrame(body);
    controls->setObjectName(QStringLiteral("recommendationControls"));
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(20, 20, 20, 20);
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
        const char *emoji;
        int minutes;
    } choices[] = {
        {VMood::Tired, "很累", "😴", 30},
        {VMood::WantGoodFood, "想吃点好的", "😋", 45},
        {VMood::LowAppetite, "没胃口", "😕", 30},
        {VMood::WantLight, "想清爽一点", "🌿", 40},
        {VMood::TreatMyself, "想犒劳自己", "🥳", 75},
    };
    for (const auto &choice : choices) {
        auto *button = new QPushButton(QStringLiteral("%1  %2").arg(QString::fromUtf8(choice.emoji), QString::fromUtf8(choice.name)), controls);
        button->setObjectName(QStringLiteral("moodChoice"));
        button->setCheckable(true);
        button->setMinimumHeight(38);
        m_moodButtons->addButton(button, static_cast<int>(choice.mood));
        moodRow->addWidget(button, 1);
        connect(button, &QPushButton::clicked, this, [this, choice] {
            if (m_moodV1 == choice.mood) return;
            const BatchRepaint batch(this);
            selectMoodV1(choice.mood);
            if (!isSignalConnected(QMetaMethod::fromSignal(&RecommendationWidget::moodSelectedV1)))
                emit moodSelected(legacyMood(choice.mood));
            emit moodSelectedV1(choice.mood);
        });
    }
    controlsLayout->addLayout(moodRow);

    controlsLayout->addSpacing(6);
    controlsLayout->addWidget(makeLabel(QStringLiteral("最多愿意做多久？"), "limitLabel", controls));
    auto *limitRow = new QHBoxLayout;
    limitRow->setSpacing(10);
    m_timeButtons = new QButtonGroup(this);
    m_timeButtons->setExclusive(true);
    for (int minutes : {15, 30, 45, -1}) {
        auto *time = new QPushButton(minutes > 0 ? QStringLiteral("%1 分钟").arg(minutes)
                                               : QStringLiteral("自定义"), controls);
        time->setObjectName("timeChoice");
        time->setCheckable(true);
        time->setMinimumHeight(38);
        m_timeButtons->addButton(time, minutes < 0 ? 0 : minutes);
        limitRow->addWidget(time);
        connect(time, &QPushButton::clicked, this, [this, minutes] {
            if (minutes > 0) m_customLimit->setValue(minutes);
            m_customLimit->setVisible(minutes < 0);
            if (minutes < 0) { m_customLimit->setFocus(); m_customLimit->selectAll(); }
        });
    }
    m_customLimit = new QSpinBox(controls);
    m_customLimit->setObjectName(QStringLiteral("customTimeLimit"));
    m_customLimit->setRange(0, 240);
    m_customLimit->setSpecialValueText(QStringLiteral("请选择时间"));
    m_customLimit->setSuffix(QStringLiteral(" 分钟"));
    m_customLimit->setValue(0);
    m_customLimit->setMaximumWidth(150);
    m_customLimit->hide();
    limitRow->addWidget(m_customLimit);
    limitRow->addStretch();
    controlsLayout->addLayout(limitRow);
    controlsLayout->addWidget(makeLabel(QStringLiteral("食材怎么安排？"), "limitLabel", controls));
    auto *policy = new QComboBox(controls);
    policy->setObjectName("inventoryPolicy");
    policy->addItems({QStringLiteral("优先现有，允许补购"), QStringLiteral("只用现有食材")});
    controlsLayout->addWidget(policy);
    m_quickLayout = new QVBoxLayout;
    m_policyHint = makeLabel(QStringLiteral("尽量利用库存，也可以推荐需要补购的菜。"), "limitHelp", controls);
    m_policyHint->setWordWrap(true);
    controlsLayout->addWidget(m_policyHint);
    connect(policy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        m_policy = static_cast<Shixu::Contracts::V1::InventoryPolicy>(i);
        resetExcludedRecipeIds();
        updateQuickButtons();
        if (m_stages->currentIndex() == 1) sendV1Request();
    });
    connect(m_customLimit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        syncTimeChoices();
        resetExcludedRecipeIds();
        updateQuickButtons();
        if (m_moodV1 != VMood::NotSelected && m_stages->currentIndex() == 1) sendV1Request();
    });

    m_selectionHint =
        makeLabel(QStringLiteral("请先选择今天的心情。"), QStringLiteral("selectionHint"), controls);
    m_selectionHint->hide();
    controlsLayout->addWidget(m_selectionHint);

    auto *generateButton = new QPushButton(QStringLiteral("为我推荐"), controls);
    generateButton->setObjectName(QStringLiteral("generateRecommendations"));
    generateButton->setCursor(Qt::PointingHandCursor);
    generateButton->setProperty("primary", true);
    generateButton->setMinimumSize(200, 46);
    controlsLayout->addWidget(generateButton, 0, Qt::AlignLeft);
    connect(generateButton, &QPushButton::clicked, this, &RecommendationWidget::requestRecommendations);
    controls->setMaximumWidth(1000);
    controls->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    bodyLayout->addWidget(controls);
    bodyLayout->addStretch();

    auto *resultPage = new QWidget(this);
    resultPage->setObjectName("recommendationResultsPage");
    auto *resultPageLayout = new QVBoxLayout(resultPage);
    resultPageLayout->setContentsMargins(0, 0, 0, 0);
    resultPageLayout->setSpacing(12);
    auto *resultScroll = new QScrollArea(resultPage);
    m_resultScroll = resultScroll;
    resultScroll->setProperty("pageScroll", true);
    resultScroll->setObjectName("recommendationResultsScroll");
    resultScroll->setWidgetResizable(true);
    resultScroll->setFrameShape(QFrame::NoFrame);
    resultScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    resultPageLayout->addWidget(resultScroll, 1);
    m_stages->addWidget(resultPage);
    auto *resultBody = new QWidget(resultScroll);
    resultBody->setObjectName("pageBody");
    auto *resultLayout = new QVBoxLayout(resultBody);
    resultLayout->setContentsMargins(0, 0, 8, 0);
    resultLayout->setSpacing(12);
    resultScroll->setWidget(resultBody);
    auto *resultHeader = new QHBoxLayout;
    resultHeader->addWidget(makeLabel(QStringLiteral("今晚为你挑好了"), "controlsTitle", resultBody));
    resultHeader->addStretch();
    auto *edit = new QPushButton(QStringLiteral("修改条件"), resultBody);
    edit->setObjectName("editRecommendationConditions");
    resultHeader->addWidget(edit);
    connect(edit, &QPushButton::clicked, this, [this] { showStage(0); });
    resultLayout->addLayout(resultHeader);
    m_conditions = makeLabel({}, "recommendationConditions", resultBody);
    m_conditions->setWordWrap(true);
    resultLayout->addWidget(m_conditions);
    m_warning = makeLabel({}, "recommendationWarnings", resultBody);
    m_warning->setWordWrap(true);
    m_warning->hide();
    resultLayout->addWidget(m_warning);
    m_summary = makeLabel({}, "recommendationSummary", resultBody);
    m_summary->hide();
    resultLayout->addWidget(m_summary);
    m_emptyState = new QFrame(resultBody);
    m_emptyState->setObjectName("recommendationCard");
    m_emptyState->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(24, 28, 24, 28);
    emptyLayout->setSpacing(12);
    m_emptyTitle = makeLabel(QStringLiteral("还没有生成推荐"), "emptyTitle", m_emptyState);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyTitle->setWordWrap(true);
    m_emptyDescription = makeLabel(QStringLiteral("选择今天的心情，我来帮你挑。"), "emptyDescription", m_emptyState);
    m_emptyDescription->setAlignment(Qt::AlignCenter);
    m_emptyDescription->setWordWrap(true);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyDescription);
    m_allowPurchase = new QPushButton(QStringLiteral("允许补购"), m_emptyState);
    m_allowPurchase->setObjectName("allowPurchase");
    m_allowPurchase->hide();
    emptyLayout->addWidget(m_allowPurchase, 0, Qt::AlignCenter);
    connect(m_allowPurchase, &QPushButton::clicked, this, [this] {
        m_policy = Shixu::Contracts::V1::InventoryPolicy::PreferInventory;
        if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
            QSignalBlocker blocker(combo); combo->setCurrentIndex(int(m_policy));
        }
        resetExcludedRecipeIds(); updateQuickButtons(); sendV1Request();
    });
    auto *undoEmpty = new QPushButton(QStringLiteral("撤销快捷调整"), m_emptyState);
    undoEmpty->setObjectName("undoEmptyRefinements");
    emptyLayout->addWidget(undoEmpty, 0, Qt::AlignCenter);
    connect(undoEmpty, &QPushButton::clicked, this, &RecommendationWidget::restoreInitialConditions);
    resultLayout->addWidget(m_emptyState);
    auto *results = new QWidget(resultBody);
    results->setObjectName("recommendationCards");
    results->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_resultsLayout = new QVBoxLayout(results);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(16);
    resultLayout->addWidget(results);
    resultLayout->addStretch();

    // The quick actions belong to this stage, outside its scroll viewport.
    auto *quickCard = new QFrame(resultPage);
    quickCard->setObjectName("recommendationQuickCard");
    quickCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *quickBox = new QVBoxLayout(quickCard);
    quickBox->setContentsMargins(16, 12, 16, 14);
    quickBox->setSpacing(10);
    auto *quickHeader = new QHBoxLayout;
    quickHeader->addWidget(makeLabel(QStringLiteral("这些还不太想吃？"), "quickTitle", quickCard));
    quickHeader->addStretch();
    auto *reset = new QPushButton(QStringLiteral("撤销快捷调整"), quickCard);
    reset->setObjectName("resetRecommendationAdjustments");
    quickHeader->addWidget(reset);
    quickBox->addLayout(quickHeader);
    quickBox->addLayout(m_quickLayout);
    connect(reset, &QPushButton::clicked, this, &RecommendationWidget::restoreInitialConditions);
    m_swapButton = new QPushButton(QStringLiteral("换一批"), quickCard);
    m_swapButton->setObjectName("swapRecommendations");
    m_swapButton->setEnabled(false);
    connect(m_swapButton, &QPushButton::clicked, this, [this] {
        m_excludedIds.insert(m_excludedIds.end(), m_visibleIds.begin(), m_visibleIds.end());
        m_visibleIds.clear();
        sendV1Request();
    });
    resultPageLayout->addWidget(quickCard);
    updateQuickButtons();
    auto *detailPage = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    auto *back = new QPushButton(QStringLiteral("← 返回推荐结果"), detailPage);
    back->setObjectName("backToRecommendations");
    detailLayout->addWidget(back, 0, Qt::AlignLeft);
    connect(back, &QPushButton::clicked, this, [this] { showStage(1); });
    m_recipeDetails = new RecipeDetailsWidget(detailPage);
    m_recipeDetails->setObjectName("recommendationRecipeDetails");
    detailLayout->addWidget(m_recipeDetails, 1);
    m_stages->addWidget(detailPage);

    setStyleSheet(QStringLiteral(R"(
        QFrame#recommendationControls, QFrame#recommendationCard, QFrame#recommendationQuickCard {
            background: white; border: 1px solid #DCEAF0; border-radius: 14px;
        }
        QFrame#recommendationCard[primaryCard="true"] { border:2px solid #79B6D8; background:#F5FBFF; }
        QLabel#cardRole { color:#527992; font-size:13px; }
        QLabel#primaryDishName { color:#244E6A; font-size:24px; font-weight:700; }
        QLabel#recommendationGaps { color:#38755C; font-size:13px; }
        QLabel#recommendationGaps[needsAttention="true"] { color:#9A4E2D; }
        QLabel#recommendationBrief { background:#EAF4FA; color:#315970; border-radius:8px; padding:12px; font-size:14px; }
        QFrame#recommendationReasonPanel { background:#EEF7FB; border:0; border-radius:9px; }
        QFrame#recommendationReasonPanel QLabel { background:transparent; }
        QLabel#reasonCaption { color:#627E90; font-size:12px; }
        QLabel#reasonTime, QLabel#reasonScore { color:#216F9A; font-size:24px; font-weight:700; }
        QLabel#reasonHeading { color:#284F69; font-size:14px; font-weight:700; }
        QLabel#reasonStock { color:#38755C; font-size:14px; font-weight:600; }
        QLabel#reasonStock[needsAttention="true"] { color:#A14F28; }
        QLabel#reasonText { color:#315970; font-size:14px; }
        QLabel#reasonHighlight { color:#216F9A; font-size:14px; font-weight:600; }
        QLabel#reasonNote { color:#627E90; font-size:12px; }
        QLabel#quickTitle { color:#244E6A; font-size:16px; font-weight:600; }
        QLabel#recommendationStage { background:white; border-radius:8px; padding:10px; color:#718A9A; }
        QLabel#recommendationStage[active="true"] { background:#DCEEF8; color:#185D88; font-weight:700; }
        QLabel#recommendationConditions { color:#315970; font-size:14px; }
        QLabel#controlsTitle, QLabel#moodHeading { color: #244E6A; font-size: 20px; font-weight: 700; }
        QLabel#controlsDescription, QLabel#limitHelp, QLabel#timeReference {
            color: #6B8797; font-size: 13px;
        }
        QLabel#limitLabel { color: #31566D; font-size: 14px; }
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
    findChild<QPushButton *>("resetRecommendationAdjustments")->setProperty("link", true);
    showStage(0);
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

void RecommendationWidget::requestRecommendations() {
    if (m_moodV1 != Shixu::Contracts::V1::Mood::NotSelected && m_customLimit->value() > 0) {
        m_initialMinutes = m_customLimit->value();
        m_initialPolicy = m_policy;
        updateQuickButtons();
        showStage(1);
        m_resultScroll->verticalScrollBar()->setValue(0);
    }
    sendV1Request();
}

RecipeDetailsWidget *RecommendationWidget::recipeDetails() const { return m_recipeDetails; }
void RecommendationWidget::showRecipeDetails() {
    m_resultScrollPosition = m_resultScroll->verticalScrollBar()->value();
    showStage(2);
    m_recipeDetails->scrollToTop();
}
void RecommendationWidget::showStage(int stage) {
    const int previous = m_stages->currentIndex();
    m_stages->setCurrentIndex(stage);
    if (previous == 2 && stage == 1) {
        const int saved = m_resultScrollPosition;
        m_resultScroll->verticalScrollBar()->setValue(saved);
        QTimer::singleShot(0, this, [this, saved] {
            if (m_stages->currentIndex() == 1) m_resultScroll->verticalScrollBar()->setValue(saved);
        });
    }
    for (int i = 0; i < m_stageLabels.size(); ++i) {
        m_stageLabels[i]->setProperty("active", i == stage);
        m_stageLabels[i]->style()->unpolish(m_stageLabels[i]);
        m_stageLabels[i]->style()->polish(m_stageLabels[i]);
    }
}
void RecommendationWidget::updateConditions() {
    const QStringList moods{QStringLiteral("很累"), QStringLiteral("想吃点好的"), QStringLiteral("没胃口"),
                            QStringLiteral("想清爽一点"), QStringLiteral("想犒劳自己")};
    const int index = int(m_moodV1) - 1;
    if (index < 0 || index >= moods.size()) { m_conditions->clear(); return; }
    m_conditions->setText(QStringLiteral("当前推荐条件：%1  ·  不超过 %2 分钟  ·  %3")
        .arg(moods[index]).arg(m_customLimit->value())
        .arg(m_policy == Shixu::Contracts::V1::InventoryPolicy::OnlyInventory
                 ? QStringLiteral("只用现有食材") : QStringLiteral("允许补购")));
    if (m_refinements.count(Shixu::Contracts::V1::Refinement::MoreSpecial))
        m_conditions->setText(m_conditions->text() + QStringLiteral("  ·  更有特色"));
}

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
        m_swapButton->setEnabled(false);
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
    syncTimeChoices();
    resetExcludedRecipeIds();
    if (auto *button = qobject_cast<QPushButton *>(m_moodButtons->button(static_cast<int>(mood))))
        button->setChecked(true);
    m_selectionHint->hide();
    updateQuickButtons();
}

void RecommendationWidget::syncTimeChoices() {
    const int minutes = m_customLimit->value();
    auto *choice = minutes > 0 ? m_timeButtons->button(minutes) : nullptr;
    m_timeButtons->button(choice ? minutes : 0)->setChecked(true);
    m_customLimit->setVisible(!choice);
}

void RecommendationWidget::restoreInitialConditions() {
    {
        QSignalBlocker blocker(m_customLimit);
        m_customLimit->setValue(m_initialMinutes);
    }
    m_policy = m_initialPolicy;
    if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
        QSignalBlocker blocker(combo); combo->setCurrentIndex(int(m_policy));
    }
    m_refinements.clear();
    syncTimeChoices();
    resetExcludedRecipeIds();
    updateQuickButtons();
    sendV1Request();
}

void RecommendationWidget::updateQuickButtons() {
    namespace V = Shixu::Contracts::V1;
    auto *row = m_quickLayout->count() ? m_quickLayout->itemAt(0)->widget() : nullptr;
    if (!row) {
        row = new QWidget(this);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        const QStringList labels{QStringLiteral("只看冰箱里的"), QStringLiteral("时间短一点"), QStringLiteral("更有特色")};
        for (int i = 0; i < labels.size(); ++i) {
            auto *button = new QPushButton(labels[i], row);
            button->setObjectName("quickAdjustment");
            button->setCheckable(true);
            layout->addWidget(button, 1);
            connect(button, &QPushButton::clicked, this, [this, i] {
                const BatchRepaint batch(this);
                if (i == 0) {
                    m_policy = m_policy == V::InventoryPolicy::OnlyInventory
                        ? V::InventoryPolicy::PreferInventory : V::InventoryPolicy::OnlyInventory;
                } else if (i == 1) {
                    QSignalBlocker blocker(m_customLimit);
                    m_customLimit->setValue(std::max(5, m_customLimit->value() - 10));
                    syncTimeChoices();
                } else {
                    if (!m_refinements.erase(V::Refinement::MoreSpecial))
                        m_refinements.insert(V::Refinement::MoreSpecial);
                }
                if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
                    QSignalBlocker blocker(combo); combo->setCurrentIndex(int(m_policy));
                }
                resetExcludedRecipeIds(); updateQuickButtons(); sendV1Request();
            });
        }
        layout->addWidget(m_swapButton, 1);
        m_quickLayout->addWidget(row);
    }
    const auto buttons = row->findChildren<QPushButton *>("quickAdjustment");
    buttons[0]->setChecked(m_policy == V::InventoryPolicy::OnlyInventory);
    buttons[0]->setToolTip(QStringLiteral("再次点击可允许补购，保留心情和时间。"));
    buttons[1]->setChecked(m_customLimit->value() > 0 && m_customLimit->value() < m_initialMinutes);
    buttons[1]->setEnabled(m_customLimit->value() > 5);
    buttons[1]->setToolTip(QStringLiteral("将当前时间上限缩短 10 分钟，最低 5 分钟。"));
    buttons[2]->setChecked(m_refinements.count(V::Refinement::MoreSpecial) > 0);
    buttons[2]->setToolTip(QStringLiteral("优先更有特色的菜；再次点击可撤销。"));
    m_policyHint->setText(m_policy == V::InventoryPolicy::OnlyInventory
        ? QStringLiteral("只推荐现有库存满足用量的菜品。")
        : QStringLiteral("尽量利用库存，也可以推荐需要补购的菜。"));
}

void RecommendationWidget::sendV1Request() {
    namespace V = Shixu::Contracts::V1;
    if (m_moodV1 == V::Mood::NotSelected || m_customLimit->value() <= 0) {
        m_selectionHint->setText(m_moodV1 == V::Mood::NotSelected
            ? QStringLiteral("请先选择今天的心情。") : QStringLiteral("请选择或输入大于 0 分钟的制作时间。"));
        m_selectionHint->show();
        return;
    }
    m_selectionHint->hide();
    updateConditions();
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
    const BatchRepaint batch(this);
    while (auto *item = m_resultsLayout->takeAt(0)) {
        if (auto *widget = item->widget())
            delete widget;
        delete item;
    }
    m_visibleIds.clear();
    m_swapButton->show();
    m_swapButton->setEnabled(result.outcome.status == V::OperationStatus::Succeeded && !result.cards.empty() && result.hasMore);
    m_swapButton->setText(QStringLiteral("换一批"));
    m_swapButton->setToolTip(result.hasMore ? QStringLiteral("保留当前条件，查看其他符合条件的菜。")
                                           : QStringLiteral("当前条件下没有更多推荐，可撤销快捷调整。"));
    const bool failed = result.outcome.status != V::OperationStatus::Succeeded;
    m_allowPurchase->setVisible(!failed && result.cards.empty() && m_policy == V::InventoryPolicy::OnlyInventory);
    m_emptyState->setVisible(failed || result.cards.empty());
    m_summary->setVisible(!failed && !result.cards.empty());
    if (failed) {
        m_emptyTitle->setText(QStringLiteral("推荐暂时不可用"));
        m_emptyDescription->setText(QString::fromUtf8(result.outcome.message.c_str()));
        return;
    }
    if (result.cards.empty()) {
        m_emptyTitle->setText(m_policy == V::InventoryPolicy::OnlyInventory
            ? QStringLiteral("当前库存暂无符合条件的菜谱") : QStringLiteral("暂时没有符合条件的菜谱"));
        m_emptyDescription->setText(QString::fromUtf8(result.outcome.message.c_str()) +
                                    QStringLiteral(" 可撤销快捷调整或修改条件后重试。"));
        return;
    }
    m_summary->setText(QStringLiteral("今晚推荐 · 还有 %1 道符合条件 · 本批 %2 道")
                           .arg(result.eligibleCount)
                           .arg(result.cards.size()));
    auto *board = new QWidget(this);
    board->setObjectName("recommendationBoard");
    board->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *columns = new QHBoxLayout(board);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);
    auto *primaryColumn = new QVBoxLayout;
    primaryColumn->setContentsMargins(0, 0, 0, 0);
    primaryColumn->setAlignment(Qt::AlignTop);
    auto *alternatives = new QVBoxLayout;
    alternatives->setContentsMargins(0, 0, 0, 0);
    alternatives->setSpacing(12);
    alternatives->setAlignment(Qt::AlignTop);
    columns->addLayout(primaryColumn, 58);
    columns->addLayout(alternatives, 42);
    m_resultsLayout->addWidget(board);
    for (const auto &card : result.cards) {
        m_visibleIds.push_back(card.recipeId);
        const QString role = card.role == V::CardRole::Primary          ? QStringLiteral("今晚首选")
                             : card.role == V::CardRole::AlternativeOne ? QStringLiteral("备选一")
                                                                        : QStringLiteral("备选二");
        const bool primary = card.role == V::CardRole::Primary;
        auto *frame = new QFrame(board);
        frame->setObjectName(QStringLiteral("recommendationCard"));
        frame->setProperty("primaryCard", primary);
        frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(20, primary ? 20 : 16, 20, primary ? 20 : 16);
        layout->setSpacing(primary ? 10 : 6);
        auto *roleRow = new QHBoxLayout;
        auto *categoryIcon = new QLabel(frame);
        categoryIcon->setObjectName("recipeCategoryIcon");
        categoryIcon->setPixmap(Shixu::Ui::foodPixmap(QString::fromUtf8(card.recipe.category.c_str()), 26, devicePixelRatioF()));
        categoryIcon->setToolTip(QString::fromUtf8(card.recipe.category.c_str()));
        roleRow->addWidget(categoryIcon);
        roleRow->addWidget(makeLabel(role, "cardRole", frame));
        roleRow->addStretch();
        layout->addLayout(roleRow);
        auto *cardTitle = makeLabel(card.dishName, primary ? "primaryDishName" : "dishName", frame);
        cardTitle->setWordWrap(true);
        layout->addWidget(cardTitle);
        QStringList gaps;
        bool unknown = false;
        for (const auto &availability : card.availability) {
            if (Shixu::isPantryStaple(availability.foodName)) continue;
            if (availability.state == V::AvailabilityState::Enough)
                continue;
            unknown |= availability.state == V::AvailabilityState::Unknown;
            const QString name = QString::fromUtf8(availability.foodName.c_str());
            QString gap = availability.knownGapGrams > 0
                ? QStringLiteral("%1 约缺 %2 克").arg(name).arg(availability.knownGapGrams, 0, 'f', 0)
                : name;
            if (availability.state == V::AvailabilityState::Unknown)
                gap += QStringLiteral("（数量待确认）");
            gaps << gap;
        }
        const QString stockState = card.availability.empty() ? QStringLiteral("库存待核对")
            : gaps.isEmpty() ? QStringLiteral("食材齐全")
            : unknown ? QStringLiteral("数量待确认") : QStringLiteral("需要补充食材");
        const double perServing = card.nutrition.calories / std::max(1, card.servings);
        const QString info = primary
            ? QStringLiteral("%1 分钟 · %2 步 · 约 %3 千卡 / 份")
                .arg(card.recipe.cookingTimeMinutes).arg(card.recipe.steps.size())
                .arg(perServing, 0, 'f', 0)
            : QStringLiteral("%1 分钟 · %2").arg(card.recipe.cookingTimeMinutes).arg(stockState);
        auto *cardInfo = makeLabel(info, "timeInfo", frame);
        cardInfo->setWordWrap(true);
        layout->addWidget(cardInfo);
        QStringList pantryNames;
        for (const auto &ingredient : card.recipe.ingredients)
            if (Shixu::isPantryStaple(ingredient.foodName)) {
                const QString name = QString::fromUtf8(ingredient.foodName.c_str());
                if (!pantryNames.contains(name)) pantryNames << name;
            }
        if (primary) {
            const QString gapText = gaps.isEmpty() ? stockState : gaps.join(QStringLiteral("；"));
            auto *gapLabel = makeLabel(gapText.size() > 90 ? gapText.left(89) + QStringLiteral("…") : gapText,
                                      "recommendationGaps", frame);
            gapLabel->setToolTip(gapText);
            gapLabel->setWordWrap(true);
            gapLabel->setProperty("needsAttention", !gaps.isEmpty());
            layout->addWidget(gapLabel);
            // Reuse the service explanation, leaving time and stock details in their own rows.
            QString brief = QStringLiteral("暂无详细推荐理由");
            if (!card.reasons.isEmpty()) {
                const auto clauses = card.reasons.front().split(QStringLiteral("；"));
                brief = clauses.front();
                if (brief.startsWith(QStringLiteral("今晚首选")) && brief.contains(QChar(0xFF1A)))
                    brief = brief.mid(brief.indexOf(QChar(0xFF1A)) + 1);
                if (clauses.size() > 2 && !clauses.last().contains(QStringLiteral("时间上限")))
                    brief += QStringLiteral("；") + clauses.last();
            }
            auto *reason = makeLabel(brief, "recommendationBrief", frame);
            reason->setWordWrap(true);
            layout->addWidget(reason);
        }
        auto *detail = new QPushButton(primary ? QStringLiteral("查看这道菜") : QStringLiteral("查看详情"), frame);
        detail->setObjectName("viewRecommendedRecipe");
        detail->setProperty("primary", primary);
        detail->setMinimumHeight(primary ? 44 : 38);
        auto *actions = new QHBoxLayout;
        actions->setSpacing(8);
        actions->addWidget(detail);
        connect(detail, &QPushButton::clicked, this, [this, id = card.recipeId] { emit recipeStepsSelected(id); });
        if (primary) primaryColumn->addWidget(frame);
        else alternatives->addWidget(frame);
        const QString collapsedText = primary ? QStringLiteral("展开推荐依据") : QStringLiteral("推荐依据");
        auto *more = new QPushButton(collapsedText, frame);
        more->setObjectName(QStringLiteral("recommendationExplanation"));
        more->setProperty("link", true);
        more->setAccessibleName(card.dishName + QStringLiteral("的推荐依据"));
        actions->addWidget(more);
        actions->addStretch();
        layout->addLayout(actions);
        QStringList features;
        const std::pair<const char *, const char *> featureNames[] = {
            {V::Tags::Quick, "快手"}, {V::Tags::Easy, "简单"}, {V::Tags::Light, "清淡"},
            {V::Tags::Appetizing, "开胃"}, {V::Tags::Hearty, "丰盛"}, {V::Tags::Special, "特色"},
            {V::Tags::VegetableRich, "蔬菜丰富"}, {V::Tags::HighProtein, "高蛋白"}};
        for (const auto &feature : featureNames)
            if (std::find(card.recipe.tags.begin(), card.recipe.tags.end(), feature.first) != card.recipe.tags.end())
                features << QString::fromUtf8(feature.second);
        QString ingredients = card.availability.empty() ? QStringLiteral("暂无食材状态")
                              : gaps.isEmpty() ? QStringLiteral("所需食材充足")
                                               : gaps.join(QStringLiteral("；"));
        const QString characteristics = features.isEmpty() ? QStringLiteral("暂无菜谱特点标签")
                                                           : features.join(QStringLiteral(" · "));
        auto *reasonPanel = new QFrame(frame);
        reasonPanel->setObjectName(QStringLiteral("recommendationReasonPanel"));
        auto *reasonLayout = new QVBoxLayout(reasonPanel);
        reasonLayout->setContentsMargins(14, 12, 14, 12);
        reasonLayout->setSpacing(12);
        auto *metrics = new QHBoxLayout;
        metrics->setSpacing(12);
        auto *timeMetric = new QVBoxLayout;
        timeMetric->setSpacing(2);
        timeMetric->addWidget(makeLabel(QStringLiteral("预计用时"), "reasonCaption", reasonPanel));
        timeMetric->addWidget(makeLabel(QStringLiteral("%1 分钟").arg(card.recipe.cookingTimeMinutes), "reasonTime", reasonPanel));
        auto *scoreMetric = new QVBoxLayout;
        scoreMetric->setSpacing(2);
        scoreMetric->addWidget(makeLabel(QStringLiteral("综合评分"), "reasonCaption", reasonPanel));
        scoreMetric->addWidget(makeLabel(QStringLiteral("%1").arg(card.totalScore, 0, 'f', 1), "reasonScore", reasonPanel));
        metrics->addLayout(timeMetric, 1);
        metrics->addLayout(scoreMetric, 1);
        reasonLayout->addLayout(metrics);
        auto addText = [reasonPanel](QVBoxLayout *section, const QString &text, const char *name) {
            auto *label = makeLabel(text, QString::fromLatin1(name), reasonPanel);
            label->setWordWrap(true);
            section->addWidget(label);
            return label;
        };
        if (card.effectiveMaxPreparationMinutes > 0)
            addText(reasonLayout, QStringLiteral("你的时间上限：%1 分钟").arg(card.effectiveMaxPreparationMinutes), "reasonNote");
        auto addSection = [reasonPanel, reasonLayout](const QString &title) {
            auto *section = new QVBoxLayout;
            section->setSpacing(4);
            section->addWidget(makeLabel(title, "reasonHeading", reasonPanel));
            reasonLayout->addLayout(section);
            return section;
        };
        auto *stockSection = addSection(QStringLiteral("食材准备"));
        addText(stockSection, ingredients, "reasonStock")->setProperty("needsAttention", !gaps.isEmpty());
        if (card.expiryUrgencyScore > 0)
            addText(stockSection, QStringLiteral("可利用临期食材"), "reasonHighlight");
        if (!pantryNames.isEmpty())
            addText(stockSection, QStringLiteral("常备调料默认已备好：%1").arg(pantryNames.join(QStringLiteral("、"))), "reasonNote");
        addText(addSection(QStringLiteral("菜谱特点")), characteristics, "reasonText");
        auto *selectionSection = addSection(QStringLiteral("为什么推荐"));
        if (card.reasons.isEmpty())
            addText(selectionSection, QStringLiteral("暂无详细推荐理由"), "reasonNote");
        // Keep the actual service explanation. Split its clauses for readability,
        // highlighting the selection/mood rationale without inventing new reasons.
        for (int i = 0; i < card.reasons.size(); ++i) {
            const auto clauses = card.reasons[i].split(QStringLiteral("；"), Qt::SkipEmptyParts);
            for (const auto &clause : clauses)
                addText(selectionSection, clause,
                        i == 0 && !clause.startsWith(QStringLiteral("预计")) ? "reasonHighlight" : "reasonText");
        }
        reasonPanel->hide();
        layout->addWidget(reasonPanel);
        more->setCheckable(true);
        connect(more, &QPushButton::toggled, reasonPanel, [reasonPanel, more, primary, collapsedText](bool open) {
            reasonPanel->setVisible(open);
            more->setText(open ? (primary ? QStringLiteral("收起推荐依据") : QStringLiteral("收起依据")) : collapsedText);
        });
    }
}

void RecommendationWidget::setDefaultInventoryPolicy(Shixu::Contracts::V1::InventoryPolicy policy) {
    if (policy == Shixu::Contracts::V1::InventoryPolicy::SmallPurchase)
        policy = Shixu::Contracts::V1::InventoryPolicy::PreferInventory;
    m_defaultPolicy = policy;
    m_policy = policy;
    if (auto *combo = findChild<QComboBox *>("inventoryPolicy")) {
        QSignalBlocker b(combo);
        combo->setCurrentIndex(int(policy));
    }
    resetExcludedRecipeIds();
    updateQuickButtons();
}
