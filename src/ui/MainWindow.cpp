#include "ui/MainWindow.h"

#include "reward/RewardSystem.h"
#include "ui/CheckInWidget.h"
#include "ui/HomeWidget.h"
#include "ui/InventoryWidget.h"
#include "ui/RecommendationWidget.h"
#include "ui/StatisticsWidget.h"
#include "inventory/inventory.h"
#include "recommendation/RecommendationContextBuilder.h"
#include "recommendation/RecommendationEngine.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

namespace {

struct PageInfo {
    MainWindow::Page id;
    QString title;
    QString description;
};

const std::array<PageInfo, static_cast<int>(MainWindow::Page::Count)> kPages{{
    {MainWindow::Page::Home, QStringLiteral("首页"), QStringLiteral("欢迎使用食序，开始安排今天的饮食。")},
    {MainWindow::Page::FoodEntry, QStringLiteral("智能录入"), QStringLiteral("食材录入功能接入后可在这里使用。")},
    {MainWindow::Page::Inventory, QStringLiteral("食材库存"), QStringLiteral("库存管理功能接入后可在这里使用。")},
    {MainWindow::Page::Recipes, QStringLiteral("菜谱与营养"), QStringLiteral("菜谱与营养分析功能接入后可在这里使用。")},
    {MainWindow::Page::Recommendations, QStringLiteral("个性化推荐"), QStringLiteral("推荐功能接入后可在这里使用。")},
    {MainWindow::Page::CheckIn, QStringLiteral("打卡成长"), QStringLiteral("每日打卡与成长记录将在这里呈现。")},
    {MainWindow::Page::Statistics, QStringLiteral("成长统计"), QStringLiteral("做饭与打卡统计将在这里呈现。")},
    {MainWindow::Page::Profile, QStringLiteral("个人中心"), QStringLiteral("个人信息功能接入后可在这里使用。")},
}};

QWidget* makePlaceholder(const PageInfo& page)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* title = new QLabel(page.title, widget);
    title->setObjectName(QStringLiteral("placeholderTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto* description = new QLabel(page.description, widget);
    description->setObjectName(QStringLiteral("placeholderDescription"));
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(description);
    return widget;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("食序"));
    resize(1280, 800);
    setMinimumSize(980, 620);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    setCentralWidget(root);

    auto* shell = new QHBoxLayout(root);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto* sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(208);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(20, 32, 20, 24);
    sidebarLayout->setSpacing(7);

    auto* appName = new QLabel(QStringLiteral("食序"), sidebar);
    appName->setObjectName(QStringLiteral("appName"));
    sidebarLayout->addWidget(appName);

    auto* appCaption = new QLabel(QStringLiteral("让每一餐更有序"), sidebar);
    appCaption->setObjectName(QStringLiteral("appCaption"));
    sidebarLayout->addWidget(appCaption);
    sidebarLayout->addSpacing(34);

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);
    m_navigationButtons.reserve(static_cast<int>(Page::Count));
    for (const auto& page : kPages) {
        const int index = static_cast<int>(page.id);
        auto* button = new QPushButton(page.title, sidebar);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(44);
        buttonGroup->addButton(button, index);
        sidebarLayout->addWidget(button);
        m_navigationButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, page] { showPage(page.id); });
    }
    sidebarLayout->addStretch();
    shell->addWidget(sidebar);

    m_content = new QFrame(root);
    m_content->setObjectName(QStringLiteral("content"));
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(32, 30, 32, 30);
    contentLayout->setSpacing(22);

    m_pageTitle = new QLabel(m_content);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    contentLayout->addWidget(m_pageTitle);

    m_pages = new QStackedWidget(m_content);
    m_pages->setObjectName(QStringLiteral("pageStack"));
    for (const auto& page : kPages) {
        m_pages->addWidget(makePlaceholder(page));
    }
    contentLayout->addWidget(m_pages, 1);
    shell->addWidget(m_content, 1);

    setStyleSheet(QStringLiteral(R"(
        QWidget#appRoot { background: #EAF5F8; color: #24465B; }
        QFrame#sidebar { background: #FCFDFB; border-right: 1px solid #DDEBF0; }
        QLabel#appName { color: #17416B; font-size: 29px; font-weight: 700; }
        QLabel#appCaption { color: #7793A2; font-size: 12px; }
        QPushButton#navButton {
            background: transparent; border: 0; border-radius: 11px;
            color: #38586C; font-size: 14px; text-align: left; padding: 0 15px;
        }
        QPushButton#navButton:hover { background: #EFF7FA; }
        QPushButton#navButton:checked {
            background: #DCEEF8; color: #1B6297; font-weight: 700;
        }
        QLabel#pageTitle { color: #244E6A; font-size: 24px; font-weight: 700; }
        QStackedWidget#pageStack {
            background: #FFFFFF; border: 1px solid #E1EDF0; border-radius: 16px;
        }
        QLabel#placeholderTitle { color: #31566D; font-size: 22px; font-weight: 600; }
        QLabel#placeholderDescription { color: #7893A0; font-size: 14px; }
    )"));

    auto* rewards = new RewardSystem(this);
    m_inventory = new InventoryManager;
    m_home = new HomeWidget(rewards, m_pages);
    m_inventoryPage = new InventoryWidget(m_pages);
    setPage(Page::Home, m_home);
    setPage(Page::Inventory, m_inventoryPage);
    connect(m_home, &HomeWidget::foodEntryRequested, this, [this] { showPage(Page::FoodEntry); });
    connect(m_home, &HomeWidget::inventoryRequested, this, [this] { showPage(Page::Inventory); });
    connect(m_home, &HomeWidget::recipesRequested, this, [this] { showPage(Page::Recipes); });
    connect(m_home, &HomeWidget::checkInRequested, this, [this] { showPage(Page::CheckIn); });
    auto* recommendations = new RecommendationWidget(m_pages);
    setPage(Page::Recommendations, recommendations);
    connect(m_home, &HomeWidget::moodSelected,
            recommendations, &RecommendationWidget::selectMood);
    connect(recommendations, &RecommendationWidget::moodSelected,
            m_home, &HomeWidget::selectMood);    connect(m_home, &HomeWidget::moodRecommendationRequested, this,
            [this, recommendations](Shixu::RecommendationEngine::Mood mood) {
        recommendations->selectMood(mood);
        showPage(Page::Recommendations);
        recommendations->requestRecommendations();
    });
    connect(recommendations, &RecommendationWidget::recommendationRequested, this,
            [this, recommendations](Shixu::RecommendationEngine::Mood mood,
                                    int maxPreparationMinutes) {
        namespace D = Shixu::RecommendationEngine;
        D::UserContext preferences;
        preferences.mood = mood;
        preferences.maxPreparationMinutes = maxPreparationMinutes;
        preferences.topK = 5;
        preferences.servings = 1;

        const D::ContextBuildResult built = D::readUserContext(*m_inventory, preferences);
        recommendations->setWarnings(built.warnings);

        const auto results = D::recommend(built.context);
        QVector<RecommendationViewItem> items;
        items.reserve(static_cast<int>(results.size()));
        for (const D::Recommendation& result : results) {
            RecommendationViewItem item;
            item.recipeId = result.recipeId;
            item.dishName = result.dishName;
            item.cookingTimeMinutes = result.recipe.cookingTimeMinutes;
            item.moodName = D::moodDisplayName(result.mood);
            item.effectiveMaxPreparationMinutes = result.effectiveMaxPreparationMinutes;
            item.totalScore = result.totalScore;
            item.ingredientMatchScore = result.ingredientMatchScore;
            item.expiryUrgencyScore = result.expiryUrgencyScore;
            item.moodMatchScore = result.moodMatchScore;
            item.nutritionScore = result.nutritionScore;
            item.nutritionScoreAvailable = result.nutritionScoreAvailable;
            item.preparationTimeScore = result.preparationTimeScore;
            item.preparationTimeReferenceMinutes = result.preparationTimeReferenceMinutes;
            item.missingIngredients = result.missingIngredients;
            item.reasons = D::explainRecommendation(result).reasons;
            items.append(item);
        }
        recommendations->setRecommendations(items);
    });
    setPage(Page::CheckIn, new CheckInWidget(rewards, m_pages));
    setPage(Page::Statistics, new StatisticsWidget(rewards, m_pages));
    showPage(Page::Home);
}

MainWindow::~MainWindow()
{
    delete m_inventory;
}

void MainWindow::addFood(const FoodCandidate& candidate)
{
    m_inventory->addFood(candidate);
    refreshInventory();
}

void MainWindow::refreshInventory()
{
    const QList<FoodCandidate> foods = m_inventory->getAllItems();
    QList<HomeFoodItem> homeFoods;
    homeFoods.reserve(foods.size());
    for (const FoodCandidate& food : foods) {
        homeFoods.append({food.name, food.quantity, food.unit, food.expiryDate});
    }
    m_home->setInventory(homeFoods);
    m_inventoryPage->setItems(foods);
}

void MainWindow::setPage(Page page, QWidget* widget)
{
    const int index = static_cast<int>(page);
    if (!widget || index < 0 || index >= m_pages->count()) {
        return;
    }

    QWidget* oldPage = m_pages->widget(index);
    if (oldPage == widget) {
        return;
    }

    const bool wasCurrent = m_pages->currentWidget() == oldPage;
    m_pages->removeWidget(oldPage);
    oldPage->deleteLater();
    m_pages->insertWidget(index, widget);
    if (wasCurrent) {
        m_pages->setCurrentWidget(widget);
    }
}

void MainWindow::showPage(Page page)
{
    const int index = static_cast<int>(page);
    if (index < 0 || index >= m_pages->count()) {
        return;
    }

    if (page == Page::Home || page == Page::Inventory) {
        refreshInventory();
    }

    m_pages->setCurrentIndex(index);
    m_navigationButtons.at(index)->setChecked(true);
    m_pageTitle->setText(kPages.at(index).title);
    const bool home = page == Page::Home;
    m_pageTitle->setVisible(!home);
    m_content->layout()->setContentsMargins(home ? 0 : 32, home ? 0 : 30,
                                            home ? 0 : 32, home ? 0 : 30);
    m_content->layout()->setSpacing(home ? 0 : 22);
    m_content->setStyleSheet(QStringLiteral("QFrame#content { background: #EDF7F9; }"));
    m_pages->setStyleSheet(home ? QStringLiteral("QStackedWidget#pageStack { background: transparent; border: 0; }")
                                : QStringLiteral("QStackedWidget#pageStack { background: #FFFFFF; border: 1px solid #E1EDF0; border-radius: 16px; }"));
}
