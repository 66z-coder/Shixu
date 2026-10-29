#include "core/AppClock.h"
#include "contracts/ActivityContracts.h"
#include <QFileInfo>
#include "ui/MainWindow.h"
#include "recognition/PackagingEntryWidget.h"

#include "inventory/inventory.h"
#include "recommendation/RecommendationContextBuilder.h"
#include "recommendation/RecommendationEngine.h"
#include "reward/RewardSystem.h"
#include "ui/CheckInWidget.h"
#include "ui/HomeWidget.h"
#include "ui/InventoryWidget.h"
#include "ui/RecommendationWidget.h"
#include "ui/StatisticsWidget.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

#include "contracts/RecommendationContract.h"
#include "contracts/ServiceFactories.h"
#include "recipe/RecipeService.h"
#include "recognition/RecognitionService.h"
#include "ui/CookingConfirmationWidget.h"
#include "ui/RecipeDetailsWidget.h"
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTimeZone>
#include <QTimer>
#include <QUuid>
namespace V = Shixu::Contracts::V1;
namespace {
bool succeeded(const V::OperationResult &r) {
    return r.status == V::OperationStatus::Succeeded || r.status == V::OperationStatus::AlreadyApplied;
}
QString text(const std::string &s) { return QString::fromUtf8(s.c_str()); }
QDateTime appNow() { return Shixu::appNow(); }
} // namespace
struct MainWindow::Services {
    std::unique_ptr<V::IInventoryService> inventory;
    std::unique_ptr<V::IRecipeService> recipes;
    std::unique_ptr<V::IRewardService> rewards;
    std::unique_ptr<V::IRecommendationService> recommendation;
    std::unique_ptr<V::ICookingCompletionService> cooking;
    std::unique_ptr<V::ICookingActivityService> activity;
    StatisticsWidget* statistics = nullptr;
    RecommendationWidget *recommendations = nullptr;
    QListWidget *pending = nullptr;
    QLabel *status = nullptr;
    QString date;
    QString migrationError;
};

namespace {

struct PageInfo {
    MainWindow::Page id;
    QString title;
    QString description;
};

const std::array<PageInfo, static_cast<int>(MainWindow::Page::Count)> kPages{{
    {MainWindow::Page::Home, QStringLiteral("首页"), QStringLiteral("欢迎使用食序，开始安排今天的饮食。")},
    {MainWindow::Page::FoodEntry, QStringLiteral("智能录入"),
     QStringLiteral("食材录入功能接入后可在这里使用。")},
    {MainWindow::Page::Inventory, QStringLiteral("食材库存"),
     QStringLiteral("库存管理功能接入后可在这里使用。")},
    {MainWindow::Page::Recipes, QStringLiteral("菜谱与营养"),
     QStringLiteral("菜谱与营养分析功能接入后可在这里使用。")},
    {MainWindow::Page::Recommendations, QStringLiteral("个性化推荐"),
     QStringLiteral("推荐功能接入后可在这里使用。")},
    {MainWindow::Page::CheckIn, QStringLiteral("打卡成长"),
     QStringLiteral("每日打卡与成长记录将在这里呈现。")},
    {MainWindow::Page::Statistics, QStringLiteral("成长统计"),
     QStringLiteral("做饭与打卡统计将在这里呈现。")},
    {MainWindow::Page::Profile, QStringLiteral("个人中心"),
     QStringLiteral("个人信息功能接入后可在这里使用。")},
}};

QWidget *makePlaceholder(const PageInfo &page) {
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto *title = new QLabel(page.title, widget);
    title->setObjectName(QStringLiteral("placeholderTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *description = new QLabel(page.description, widget);
    description->setObjectName(QStringLiteral("placeholderDescription"));
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(description);
    return widget;
}

} // namespace

MainWindow::MainWindow(QWidget *parent, const QString &rewardFile) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("食序"));
    resize(1280, 800);
    setMinimumSize(980, 620);

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    setCentralWidget(root);

    auto *shell = new QHBoxLayout(root);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(208);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(20, 32, 20, 24);
    sidebarLayout->setSpacing(7);

    auto *appName = new QLabel(QStringLiteral("食序"), sidebar);
    appName->setObjectName(QStringLiteral("appName"));
    sidebarLayout->addWidget(appName);

    auto *appCaption = new QLabel(QStringLiteral("让每一餐更有序"), sidebar);
    appCaption->setObjectName(QStringLiteral("appCaption"));
    sidebarLayout->addWidget(appCaption);
    sidebarLayout->addSpacing(34);

    auto *buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);
    m_navigationButtons.reserve(static_cast<int>(Page::Count));
    for (const auto &page : kPages) {
        const int index = static_cast<int>(page.id);
        auto *button = new QPushButton(page.title, sidebar);
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
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(32, 30, 32, 30);
    contentLayout->setSpacing(22);

    m_pageTitle = new QLabel(m_content);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    contentLayout->addWidget(m_pageTitle);

    m_pages = new QStackedWidget(m_content);
    m_pages->setObjectName(QStringLiteral("pageStack"));
    for (const auto &page : kPages) {
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

    auto *rewards = new RewardSystem(this, rewardFile);
    m_inventory = new InventoryManager;
    auto *entry = new PackagingEntryWidget(m_pages);
    setPage(Page::FoodEntry, entry);
    connect(entry, &PackagingEntryWidget::foodsConfirmed, this,
            [this, entry](const std::vector<FoodCandidate> &foods) {
                QList<FoodCandidate> batch;
                for (const auto &food : foods)
                    batch.append(food);
                QString error;
                const bool saved = m_inventory->addFoods(batch, &error);
                entry->completeSubmission(saved, saved ? QString() : error);
                if (saved)
                    refreshInventory();
            });
    m_home = new HomeWidget(rewards, m_pages);
    m_inventoryPage = new InventoryWidget(m_pages);
    setPage(Page::Home, m_home);
    setPage(Page::Inventory, m_inventoryPage);
    connect(m_home, &HomeWidget::foodEntryRequested, this, [this] { showPage(Page::FoodEntry); });
    connect(m_home, &HomeWidget::inventoryRequested, this, [this] { showPage(Page::Inventory); });
    connect(m_home, &HomeWidget::recipesRequested, this, [this] { showPage(Page::Recipes); });
    connect(m_home, &HomeWidget::checkInRequested, this, [this] { showPage(Page::CheckIn); });
    auto *recommendations = new RecommendationWidget(m_pages);
    setPage(Page::Recommendations, recommendations);
    initializeV1(rewards, recommendations);
    setPage(Page::CheckIn, new CheckInWidget(rewards, m_pages));
    m_services->statistics = new StatisticsWidget(rewards, m_pages, m_services->activity.get());
    setPage(Page::Statistics, m_services->statistics);
    showPage(Page::Home);
    refreshPending();
}

MainWindow::~MainWindow() {
    m_services.reset();
    delete m_inventory;
}

void MainWindow::initializeV1(RewardSystem *rewards, RecommendationWidget *recommendations) {
    m_services = std::make_unique<Services>();
    auto &s = *m_services;
    RecognitionService normalizer;
    m_inventory->normalizeNames([&](const QString &name) { return normalizer.normalizeFoodName(name); },
                                &s.migrationError);
    s.inventory = V::createInventoryService(*m_inventory);
    s.recipes =
        V::createRecipeService(QDir::current().absoluteFilePath("meal_records.txt").toUtf8().toStdString());
    s.rewards = V::createRewardService(*rewards);
    s.recommendation = V::createRecommendationService(*s.recipes, *s.inventory);
    s.cooking = V::createCookingCompletionService(
        *s.inventory, *s.recipes, *s.rewards,
        QDir::current().absoluteFilePath("cooking_events.sqlite").toUtf8().toStdString());
    s.activity = V::createCookingActivityService(*s.recipes, *s.inventory, *s.cooking,
        QFileInfo(rewards->storageFilePath()).dir().filePath("cooking_achievements.json").toUtf8().toStdString());
    s.recommendations = recommendations;
    s.date = appNow().date().toString(Qt::ISODate);
    connect(m_home, &HomeWidget::moodSelectedV1, this, [recommendations](V::Mood mood) {
        QSignalBlocker blocker(recommendations);
        recommendations->selectMoodV1(mood);
    });
    connect(recommendations, &RecommendationWidget::moodSelectedV1, this, [this](V::Mood mood) {
        QSignalBlocker blocker(m_home);
        m_home->selectMoodV1(mood);
    });
    connect(m_home, &HomeWidget::moodRecommendationRequestedV1, this, [this, recommendations](V::Mood mood) {
        recommendations->selectMoodV1(mood);
        showPage(Page::Recommendations);
        recommendations->requestRecommendations();
    });
    connect(recommendations, &RecommendationWidget::recommendationRequestedV1, this,
            [this, recommendations](V::RecommendationPreferences preferences) {
                V::RecommendationResult result;
                const auto today = appNow().date().toString(Qt::ISODate);
                if (today != m_services->date) {
                    m_services->date = today;
                    recommendations->resetExcludedRecipeIds();
                    preferences.excludedRecipeIds.clear();
                }
                auto inventory = m_services->inventory->getInventoryBatches();
                if (!m_services->migrationError.isEmpty())
                    result.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure,
                                      m_services->migrationError.toUtf8().toStdString()};
                else if (!succeeded(inventory.outcome))
                    result.outcome = inventory.outcome;
                else {
                    preferences.referenceDate = appNow().date().toString(Qt::ISODate).toStdString();
                    Shixu::RecommendationEngine::UserContext context;
                    context.preferences = preferences;
                    context.inventorySnapshot = inventory.value;
                    result = m_services->recommendation->recommend(context);
                }
                recommendations->setRecommendationResult(result);
            });
    connect(recommendations, &RecommendationWidget::recipeSelected, this, &MainWindow::openRecipe);
    auto *page = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(page);
    s.status = new QLabel(page);
    s.status->setWordWrap(true);
    layout->addWidget(s.status);
    s.pending = new QListWidget(page);
    s.pending->setObjectName("pendingCookingEvents");
    layout->addWidget(s.pending);
    auto *resume = new QPushButton(QStringLiteral("打开选中的待处理记录"), page);
    layout->addWidget(resume);
    connect(resume, &QPushButton::clicked, this, [this] {
        if (auto *item = m_services->pending->currentItem())
            openCooking(0, item->data(Qt::UserRole).toString());
    });
    auto *recipes = new QListWidget(page);
    recipes->setObjectName("recipeCatalog");
    layout->addWidget(recipes, 2);
    const auto all = s.recipes->getAllRecipes();
    if (succeeded(all.outcome))
        for (const auto &recipe : all.value) {
            auto *item = new QListWidgetItem(
                QStringLiteral("%1 · %2 分钟").arg(text(recipe.name)).arg(recipe.cookingTimeMinutes),
                recipes);
            item->setData(Qt::UserRole, recipe.id);
        }
    auto *details = new QPushButton(QStringLiteral("查看选中菜谱"), page);
    layout->addWidget(details);
    connect(details, &QPushButton::clicked, this, [this, recipes] {
        if (auto *item = recipes->currentItem())
            openRecipe(item->data(Qt::UserRole).toInt());
    });
    connect(recipes, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) { openRecipe(item->data(Qt::UserRole).toInt()); });
    setPage(Page::Recipes, page);
    // A visible persistent entry survives restarts; no automatic deduction on
    // startup.
    auto *pendingButton = new QPushButton(QStringLiteral("待处理做饭记录"), m_content);
    pendingButton->setObjectName("pendingCookingButton");
    qobject_cast<QVBoxLayout *>(m_content->layout())->insertWidget(1, pendingButton);
    connect(pendingButton, &QPushButton::clicked, this, [this] {
        refreshPending();
        showPage(Page::Recipes);
    });
    auto *timer = new QTimer(this);
    timer->setInterval(30000);
    connect(timer, &QTimer::timeout, this, [this] {
        auto date = appNow().date().toString(Qt::ISODate);
        if (date != m_services->date) {
            m_services->date = date;
            m_services->recommendations->resetExcludedRecipeIds();
        }
    });
    timer->start();
}

void MainWindow::refreshPending() {
    if (m_services && m_services->statistics) m_services->statistics->refresh();
    if (!m_services)
        return;
    auto pending = m_services->cooking->getPendingCompletions();
    m_services->pending->clear();
    if (!succeeded(pending.outcome)) {
        m_services->status->setText(text(pending.outcome.message));
        return;
    }
    m_services->status->setText(
        QStringLiteral("待处理做饭记录：%1。打开后可继续结算或重新确认。").arg(pending.value.size()));
    for (const auto &event : pending.value) {
        auto detail = m_services->cooking->getCompletion(event.eventId);
        if (!succeeded(detail.outcome)) {
            m_services->status->setText(text(detail.outcome.message));
            continue;
        }
        auto *item = new QListWidgetItem(text(detail.value.snapshot.recipe.name) + " · " +
                                             text(detail.value.snapshot.request.completedAt),
                                         m_services->pending);
        item->setData(Qt::UserRole, text(event.eventId));
    }
}

void MainWindow::openRecipe(int recipeId) {
    auto recipe = m_services->recipes->findRecipeById(recipeId);
    auto stock = m_services->inventory->getInventoryBatches();
    if (!succeeded(recipe.outcome) || !succeeded(stock.outcome)) {
        QMessageBox::warning(
            this, QStringLiteral("无法加载菜谱"),
            text(!succeeded(recipe.outcome) ? recipe.outcome.message : stock.outcome.message));
        return;
    }
    V::EvaluationRequest request;
    request.inventory = stock.value;
    request.referenceDate = appNow().date().toString(Qt::ISODate).toStdString();
    for (const auto &i : recipe.value.ingredients)
        request.requirements.push_back({i.foodName, i.amountGrams});
    auto available = m_services->inventory->evaluateRequirements(request);
    if (!succeeded(available.outcome)) {
        QMessageBox::warning(this, QStringLiteral("库存读取失败"), text(available.outcome.message));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("菜谱详情"));
    dialog.resize(700, 650);
    auto *layout = new QVBoxLayout(&dialog);
    auto *widget = new RecipeDetailsWidget(&dialog);
    layout->addWidget(widget);
    widget->setRecipe(recipe.value, calculateNutrition(recipe.value, 1), available.value);
    bool cooking = false;
    connect(widget, &RecipeDetailsWidget::cookingRequested, &dialog, [&](int) {
        cooking = true;
        dialog.accept();
    });
    dialog.exec();
    if (cooking)
        openCooking(recipeId);
}

void MainWindow::openCooking(int recipeId, const QString &eventId) {
    if (!m_services->migrationError.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("库存迁移失败"), m_services->migrationError);
        return;
    }
    V::CookingCompletionRequest request;
    V::CookingCompletionDetails stored;
    if (!eventId.isEmpty()) {
        auto details = m_services->cooking->getCompletion(eventId.toStdString());
        if (!succeeded(details.outcome)) {
            QMessageBox::warning(this, QStringLiteral("无法恢复记录"), text(details.outcome.message));
            return;
        }
        stored = details.value;
        request = stored.snapshot.request;
    } else {
        request.eventId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        request.recipeId = recipeId;
        request.servings = 1;
        request.completedAt = appNow().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("做饭确认与恢复"));
    dialog.resize(750, 650);
    auto *layout = new QVBoxLayout(&dialog);
    auto *widget = new CookingConfirmationWidget(&dialog);
    layout->addWidget(widget);
    auto preview = [&](const V::CookingCompletionRequest &r) {
        widget->setBusy(true);
        auto p = m_services->cooking->previewCompletion(r);
        widget->setPreview(p);
        widget->setBusy(false);
    };
    auto progress = [&](const V::CookingCompletionResult &r) {
        widget->setProgress(r);
        widget->setBusy(false);
        refreshInventory();
        refreshPending();
        if (r.outcome.status == V::OperationStatus::NeedsConfirmation) {
            auto saved = m_services->cooking->getCompletion(r.eventId);
            if (succeeded(saved.outcome))
                preview(saved.value.snapshot.request);
        }
    };
    // Queued delivery avoids destroying a checkbox while its toggled signal is
    // running.
    connect(widget, &CookingConfirmationWidget::previewRequested, &dialog,
            [&](const V::CookingCompletionRequest &r) {
                auto copy = r;
                QTimer::singleShot(0, &dialog, [&, copy] { preview(copy); });
            });
    connect(widget, &CookingConfirmationWidget::completionRequested, &dialog,
            [&](const V::CookingCompletionRequest &r, const QString &token) {
                widget->setBusy(true);
                progress(m_services->cooking->completeCooking(r, token.toStdString()));
            });
    connect(widget, &CookingConfirmationWidget::retryRequested, &dialog, [&](const QString &id) {
        widget->setBusy(true);
        progress(m_services->cooking->retryCompletion(id.toStdString()));
    });
    connect(widget, &CookingConfirmationWidget::cancelPreparedRequested, &dialog, [&](const QString &id) {
        widget->setBusy(true);
        progress(m_services->cooking->cancelPreparedCompletion(id.toStdString()));
    });
    if (eventId.isEmpty())
        preview(request);
    else {
        V::CookingCompletionPreview display;
        display.snapshot = stored.snapshot;
        display.outcome = stored.progress.outcome;
        widget->setPreview(display);
        widget->setProgress(stored.progress);
    }
    auto *close = new QPushButton(QStringLiteral("关闭（未完成记录会保留）"), &dialog);
    layout->addWidget(close);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    refreshPending();
}

void MainWindow::addFood(const FoodCandidate &candidate) {
    m_inventory->addFood(candidate);
    refreshInventory();
}

void MainWindow::refreshInventory() {
    if (m_services)
        m_services->recommendations->resetExcludedRecipeIds();
    const QList<FoodCandidate> foods = m_inventory->getAllItems();
    QList<HomeFoodItem> homeFoods;
    homeFoods.reserve(foods.size());
    for (const FoodCandidate &food : foods) {
        homeFoods.append({food.name, food.quantity, food.unit, food.expiryDate});
    }
    m_home->setInventory(homeFoods);
    m_inventoryPage->setItems(foods);
}

void MainWindow::setPage(Page page, QWidget *widget) {
    const int index = static_cast<int>(page);
    if (!widget || index < 0 || index >= m_pages->count()) {
        return;
    }

    QWidget *oldPage = m_pages->widget(index);
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

void MainWindow::showPage(Page page) {
    if (page == Page::Statistics && m_services && m_services->statistics) m_services->statistics->refresh();
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
    m_content->layout()->setContentsMargins(home ? 0 : 32, home ? 0 : 30, home ? 0 : 32, home ? 0 : 30);
    m_content->layout()->setSpacing(home ? 0 : 22);
    m_content->setStyleSheet(QStringLiteral("QFrame#content { background: #EDF7F9; }"));
    m_pages->setStyleSheet(home
                               ? QStringLiteral("QStackedWidget#pageStack { background: "
                                                "transparent; border: 0; }")
                               : QStringLiteral("QStackedWidget#pageStack { background: #FFFFFF; border: 1px "
                                                "solid #E1EDF0; border-radius: 16px; }"));
}
