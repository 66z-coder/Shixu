#include "ui/MainWindow.h"
#include "contracts/ActivityContracts.h"
#include "core/AppClock.h"
#include "core/PantryStaples.h"
#include "core/UserProfile.h"
#include "recognition/PackagingEntryWidget.h"
#include "ui/AppTheme.h"
#include "ui/AppIcons.h"
#include <QComboBox>
#include <QBuffer>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QSet>

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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <algorithm>

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
class DoubleClickLabel final : public QLabel {
  public:
    using QLabel::QLabel;
    std::function<void()> onDoubleClick;
  protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && onDoubleClick) onDoubleClick();
        else QLabel::mouseDoubleClickEvent(event);
    }
};
class ExcludedFoodsWidget final : public QWidget {
  public:
    explicit ExcludedFoodsWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("profileExcluded"));
        m_grid = new QGridLayout(this);
        m_grid->setContentsMargins(0, 4, 0, 4);
        m_grid->setHorizontalSpacing(18);
        m_grid->setVerticalSpacing(10);
    }
    void addFood(const QString &name, bool checked) {
        auto *box = new QCheckBox(name, this);
        box->setObjectName(QStringLiteral("profileExcludedOption"));
        box->setChecked(checked);
        m_options.append(box);
        m_grid->addWidget(box, (m_options.size() - 1) / m_columns, (m_options.size() - 1) % m_columns);
    }
    QStringList selected() const {
        QStringList result;
        for (auto *box : m_options) if (box->isChecked()) result.append(box->text());
        return result;
    }
  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const int columns = qBound(2, event->size().width() / 155, 5);
        if (columns == m_columns) return;
        m_columns = columns;
        for (int i = 0; i < m_options.size(); ++i)
            m_grid->addWidget(m_options[i], i / m_columns, i % m_columns);
    }
  private:
    QGridLayout *m_grid = nullptr;
    QVector<QCheckBox *> m_options;
    int m_columns = 2;
};
QPixmap profileAvatar(const UserProfile &profile) {
    QPixmap output(96, 96);
    output.fill(Qt::transparent);
    QPainter p(&output);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(QRectF(1, 1, 94, 94));
    QImage image;
    if (!profile.avatarImage.isEmpty())
        image.loadFromData(QByteArray::fromBase64(profile.avatarImage.toLatin1()), "PNG");
    if (!image.isNull()) {
        p.setClipPath(circle);
        auto scaled = image.scaled(94, 94, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawImage(QPointF(1 - (scaled.width() - 94) / 2.0, 1 - (scaled.height() - 94) / 2.0), scaled);
    } else {
        const QStringList colors{"#D9ECF8", "#FBE0DC", "#DDEFE5", "#F4EDD9"};
        const QStringList names{QStringLiteral("厨"), QStringLiteral("茄"), QStringLiteral("叶"), QStringLiteral("饭")};
        p.fillPath(circle, QColor(colors[profile.avatar]));
        p.setPen(QColor("#315970"));
        QFont font = p.font(); font.setPixelSize(34); p.setFont(font);
        p.drawText(output.rect(), Qt::AlignCenter, names[profile.avatar]);
    }
    return output;
}
QDateTime appNow() { return Shixu::appNow(); }
} // namespace
struct MainWindow::Services {
    std::unique_ptr<V::IInventoryService> inventory;
    std::unique_ptr<V::IRecipeService> recipes;
    std::unique_ptr<V::IRewardService> rewards;
    std::unique_ptr<V::IRecommendationService> recommendation;
    std::unique_ptr<V::ICookingCompletionService> cooking;
    std::unique_ptr<V::ICookingActivityService> activity;
    std::unique_ptr<UserProfileStore> profile;
    QWidget *pendingArea = nullptr;
    StatisticsWidget *statistics = nullptr;
    RecommendationWidget *recommendations = nullptr;
    QListWidget *pending = nullptr;
    QLabel *status = nullptr;
    QListWidget *catalog = nullptr;
    RecipeDetailsWidget *recipeDetail = nullptr;
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
    {MainWindow::Page::Statistics, QStringLiteral("我的成长"),
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
    Shixu::applyTheme();
    setWindowTitle(QStringLiteral("食序"));
    QApplication::setWindowIcon(QIcon(":/icons/shixu.png"));
    setWindowIcon(QIcon(":/icons/shixu.png"));
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
    auto* brandIcon=new QLabel(sidebar);brandIcon->setPixmap(QPixmap(":/icons/shixu.png").scaled(48,48,Qt::KeepAspectRatio,Qt::SmoothTransformation));sidebarLayout->addWidget(brandIcon);
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
        const Shixu::Ui::Icon icons[] = {Shixu::Ui::Icon::Home, Shixu::Ui::Icon::Scan,
            Shixu::Ui::Icon::Fridge, Shixu::Ui::Icon::Recipes, Shixu::Ui::Icon::Spark,
            Shixu::Ui::Icon::Growth, Shixu::Ui::Icon::Growth, Shixu::Ui::Icon::Profile};
        button->setIcon(Shixu::Ui::icon(icons[index]));
        button->setIconSize(QSize(20, 20));
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
        QWidget#appRoot { background: #EDF7F9; color: #28485D; }
        QFrame#sidebar { background: #FCFDFB; border-right: 1px solid #DDEBF0; }
        QLabel#appName { color: #17416B; font-size: 29px; font-weight: 700; }
        QLabel#appCaption { color: #7793A2; font-size: 12px; }
        QPushButton#navButton {
            background: transparent; border: 0; border-radius: 10px;
            color: #38586C; font-size: 14px; text-align: left; padding: 0 12px; min-height:44px;
        }
        QPushButton#navButton:hover { background: #EFF7FA; }
        QPushButton#navButton:focus { border:1px solid #7CB5D6; }
        QPushButton#navButton:checked {
            background: #DCEEF8; color: #1B6297; font-weight: 700;
        }
        QLabel#pageTitle { color: #244E6A; font-size: 24px; font-weight: 700; }
        QStackedWidget#pageStack {
            background: transparent; border:0;
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
    m_inventoryPage->setUpdateHandler([this](FoodCandidate food, QString *error) {
        RecognitionService normalizer;
        food.name = normalizer.normalizeFoodName(food.name);
        food.unit = normalizer.normalizeUnit(food.unit);
        if (!m_inventory->updateFood(food, error)) return false;
        refreshInventory();
        if (m_services && m_services->recommendations)
            m_services->recommendations->requestRecommendations();
        return true;
    });
    setPage(Page::Home, m_home);
    setPage(Page::Inventory, m_inventoryPage);
    connect(m_home, &HomeWidget::foodEntryRequested, this, [this] { showPage(Page::FoodEntry); });
    connect(m_home, &HomeWidget::inventoryRequested, this, [this] { showPage(Page::Inventory); });
    connect(m_home, &HomeWidget::recipesRequested, this, [this] { showPage(Page::Recipes); });
    connect(m_home, &HomeWidget::checkInRequested, this, [this] { showPage(Page::CheckIn); });
    auto *recommendations = new RecommendationWidget(m_pages);
    setPage(Page::Recommendations, recommendations);
    initializeV1(rewards, recommendations);
    m_navigationButtons.at(static_cast<int>(Page::CheckIn))->hide();
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
                                                 QFileInfo(rewards->storageFilePath())
                                                     .dir()
                                                     .filePath("cooking_achievements.json")
                                                     .toUtf8()
                                                     .toStdString());
    s.profile = std::make_unique<UserProfileStore>(
        QFileInfo(rewards->storageFilePath()).dir().filePath("profile.json"));
    recommendations->setDefaultInventoryPolicy(s.profile->value.policy);
    auto *profilePage = new QScrollArea(m_pages);
    profilePage->setObjectName("profileScroll");
    profilePage->setProperty("pageScroll", true);
    profilePage->setFrameShape(QFrame::NoFrame);
    profilePage->setWidgetResizable(true);
    profilePage->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *profileBody = new QWidget(profilePage);
    profileBody->setObjectName("profileBody");
    profilePage->setWidget(profileBody);
    auto *profileLayout = new QVBoxLayout(profileBody);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(16);
    profileBody->setStyleSheet(QStringLiteral(R"(
        QWidget#profileBody { background:#EDF7F9; }
        QFrame#profileCard { background:white; border:1px solid #DCE8EF; border-radius:14px; }
        QLabel#profileHeading { color:#244E6A; font-size:18px; font-weight:700; }
        QLabel#profileCaption { color:#627E90; font-size:12px; }
        QLabel#profileNickname { color:#244E6A; font-size:23px; font-weight:700; }
        QLabel#profileStatus { color:#527992; }
        QCheckBox#profileExcludedOption { background:transparent; padding:3px 0; spacing:7px; }
    )"));
    auto makeProfileCard = [&](const QString &title) {
        auto *frame = new QFrame(profileBody);
        frame->setObjectName(QStringLiteral("profileCard"));
        auto *box = new QVBoxLayout(frame);
        box->setContentsMargins(20, 20, 20, 20);
        box->setSpacing(10);
        auto *heading = new QLabel(title, frame);
        heading->setObjectName(QStringLiteral("profileHeading"));
        box->addWidget(heading);
        profileLayout->addWidget(frame);
        return box;
    };
    auto *detailsLayout = makeProfileCard(QStringLiteral("个人资料"));
    auto *profileRow = new QHBoxLayout;
    profileRow->setSpacing(20);
    auto *avatar = new DoubleClickLabel(profileBody);
    avatar->setObjectName("profileAvatar");
    avatar->setFixedSize(96, 96);
    avatar->setCursor(Qt::PointingHandCursor);
    avatar->setToolTip(QStringLiteral("双击选择头像图片"));
    avatar->setPixmap(profileAvatar(s.profile->value));
    profileRow->addWidget(avatar);
    auto *identity = new QVBoxLayout;
    identity->setSpacing(6);
    identity->addStretch();
    auto *nickname = new DoubleClickLabel(s.profile->value.nickname, profileBody);
    nickname->setObjectName("profileNickname");
    nickname->setCursor(Qt::PointingHandCursor);
    nickname->setToolTip(QStringLiteral("双击修改昵称"));
    identity->addWidget(nickname);
    auto *identityHint = new QLabel(QStringLiteral("双击头像或昵称修改"), profileBody);
    identityHint->setObjectName("profileCaption");
    identity->addWidget(identityHint);
    identity->addStretch();
    profileRow->addLayout(identity, 1);
    detailsLayout->addLayout(profileRow);
    auto *preferencesLayout = makeProfileCard(QStringLiteral("饮食偏好"));
    auto *policy = new QComboBox(profileBody);
    policy->addItems(
        {QStringLiteral("优先现有，允许补购"), QStringLiteral("只用现有食材")});
    policy->setCurrentIndex(int(s.profile->value.policy));
    preferencesLayout->addWidget(new QLabel(QStringLiteral("默认库存策略"), profileBody));
    preferencesLayout->addWidget(policy);
    preferencesLayout->addWidget(new QLabel(QStringLiteral("避开食材"), profileBody));
    auto *excludedCaption = new QLabel(QStringLiteral("勾选后，推荐中会排除含同名食材的菜谱。"), profileBody);
    excludedCaption->setObjectName(QStringLiteral("profileCaption"));
    excludedCaption->setWordWrap(true);
    preferencesLayout->addWidget(excludedCaption);
    auto *excluded = new ExcludedFoodsWidget(profileBody);
    QSet<QString> names;
    const auto recipeQuery = s.recipes->getAllRecipes();
    for (const auto &recipe : recipeQuery.value)
        for (const auto &ingredient : recipe.ingredients)
            names.insert(text(ingredient.foodName));
    for (const auto &name : s.profile->value.excluded)
        names.insert(name);
    auto sorted = names.values();
    sorted.sort();
    for (const auto &name : sorted)
        excluded->addFood(name, s.profile->value.excluded.contains(name));
    preferencesLayout->addWidget(excluded);
    auto *profileStatus = new QLabel(s.profile->error, profileBody);
    profileStatus->setObjectName(QStringLiteral("profileStatus"));
    profileStatus->setWordWrap(true);
    preferencesLayout->addWidget(profileStatus);
    nickname->onDoubleClick = [this, nickname] {
        bool accepted = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("修改昵称"), QStringLiteral("昵称（最多 30 字）"),
                                                  QLineEdit::Normal, m_services->profile->value.nickname, &accepted);
        if (!accepted) return;
        auto next = m_services->profile->value;
        next.nickname = name;
        if (!m_services->profile->save(next)) {
            QMessageBox::warning(this, QStringLiteral("无法保存昵称"), QStringLiteral("请检查昵称和资料文件，原昵称已保留。"));
            return;
        }
        nickname->setText(m_services->profile->value.nickname);
    };
    avatar->onDoubleClick = [this, avatar] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择头像"), {},
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (path.isEmpty()) return;
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (QFileInfo(path).size() > 10 * 1024 * 1024 || !size.isValid() ||
            qint64(size.width()) * size.height() > 40000000) {
            QMessageBox::warning(this, QStringLiteral("无法导入头像"), QStringLiteral("请选择不超过 10 MB 且尺寸适中的图片。"));
            return;
        }
        reader.setScaledSize(size.scaled(256, 256, Qt::KeepAspectRatio));
        const QImage image = reader.read();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (image.isNull() || !buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            QMessageBox::warning(this, QStringLiteral("无法导入头像"), QStringLiteral("无法读取图片，请选择其他图片。"));
            return;
        }
        auto next = m_services->profile->value;
        next.avatarImage = QString::fromLatin1(bytes.toBase64());
        if (!m_services->profile->save(next)) {
            QMessageBox::warning(this, QStringLiteral("无法保存头像"), QStringLiteral("资料文件无法保存，原头像已保留。"));
            return;
        }
        avatar->setPixmap(profileAvatar(m_services->profile->value));
    };
    auto *saveProfile = new QPushButton(QStringLiteral("保存饮食偏好"), profileBody);
    saveProfile->setObjectName("saveProfile");
    saveProfile->setProperty("primary", true);
    saveProfile->setEnabled(s.profile->error.isEmpty() && succeeded(recipeQuery.outcome));
    preferencesLayout->addWidget(saveProfile, 0, Qt::AlignLeft);
    profileLayout->addStretch();
    setPage(Page::Profile, profilePage);
    s.profile->changed = [this, recommendations] {
        recommendations->setDefaultInventoryPolicy(m_services->profile->value.policy);
        recommendations->resetExcludedRecipeIds();
        recommendations->requestRecommendations();
    };
    connect(saveProfile, &QPushButton::clicked, this,
            [this, policy, excluded, profileStatus] {
                UserProfile next = m_services->profile->value;
                next.excluded = excluded->selected();
                next.policy = static_cast<V::InventoryPolicy>(policy->currentIndex());
                profileStatus->setText(
                    m_services->profile->save(next)
                        ? QStringLiteral("已保存，下次打开仍会保留。推荐已更新。")
                        : QStringLiteral("保存失败，请检查昵称或文件写入权限；原资料已保留。"));
            });
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
                if (!m_services->profile->error.isEmpty())
                    result.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure,
                                      m_services->profile->error.toStdString()};
                else if (!m_services->migrationError.isEmpty())
                    result.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure,
                                      m_services->migrationError.toUtf8().toStdString()};
                else if (!succeeded(inventory.outcome))
                    result.outcome = inventory.outcome;
                else {
                    preferences.referenceDate = appNow().date().toString(Qt::ISODate).toStdString();
                    Shixu::RecommendationEngine::UserContext context;
                    context.preferences = preferences;
                    context.excludedIngredients = m_services->profile->value.excluded;
                    context.inventorySnapshot = inventory.value;
                    const auto activity = m_services->activity->refresh(preferences.referenceDate);
                    if (succeeded(activity.outcome)) {
                        const QDate earliest = appNow().date().addDays(-2);
                        for (const auto &meal : activity.value.recentMeals) {
                            const QDate date = QDate::fromString(text(meal.date), Qt::ISODate);
                            if (!date.isValid() || date > appNow().date()) continue;
                            if (date < earliest) break;
                            context.recentRecipeIds.push_back(meal.recipeId);
                            if (context.recentRecipeIds.size() == 3) break;
                        }
                    }
                    result = m_services->recommendation->recommend(context);
                }
                recommendations->setRecommendationResult(result);
            });
    auto openRecommendedRecipe = [this, recommendations](int id) {
        if (loadRecipeDetails(recommendations->recipeDetails(), id))
            recommendations->showRecipeDetails();
    };
    connect(recommendations, &RecommendationWidget::recipeSelected, this, openRecommendedRecipe);
    connect(recommendations, &RecommendationWidget::recipeStepsSelected, this, openRecommendedRecipe);
    connect(recommendations->recipeDetails(), &RecipeDetailsWidget::cookingRequested, this,
            [this, recommendations](int id) {
                openCooking(id);
                loadRecipeDetails(recommendations->recipeDetails(), id);
            });
    auto *page = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    s.pendingArea = new QWidget(page);
    auto *pendingLayout = new QVBoxLayout(s.pendingArea);
    pendingLayout->setContentsMargins(0, 0, 0, 0);
    s.status = new QLabel(s.pendingArea);
    s.status->setWordWrap(true);
    pendingLayout->addWidget(s.status);
    s.pending = new QListWidget(s.pendingArea);
    s.pending->setObjectName("pendingCookingEvents");
    s.pending->setMaximumHeight(150);
    pendingLayout->addWidget(s.pending);
    auto *resume = new QPushButton(QStringLiteral("打开选中的待处理记录"), s.pendingArea);
    pendingLayout->addWidget(resume);
    layout->addWidget(s.pendingArea);
    connect(resume, &QPushButton::clicked, this, [this] {
        if (auto *item = m_services->pending->currentItem())
            openCooking(0, item->data(Qt::UserRole).toString());
    });
    layout->addWidget(new QLabel(QStringLiteral("挑选今天想吃的菜，查看营养与做法"), page));
    auto *columns = new QHBoxLayout;
    columns->setSpacing(16);
    auto *left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->addWidget(new QLabel(QStringLiteral("选择菜谱"), page));
    auto *recipes = new QListWidget(page);
    recipes->setObjectName("recipeCatalog");
    recipes->setIconSize(QSize(24, 24));
    recipes->setWordWrap(true);
    recipes->setTextElideMode(Qt::ElideNone);
    recipes->setResizeMode(QListView::Adjust);
    recipes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    s.catalog = recipes;
    left->addWidget(recipes, 1);
    auto *listPanel = new QWidget(page);
    listPanel->setLayout(left);
    listPanel->setMinimumWidth(248);
    listPanel->setMaximumWidth(340);
    columns->addWidget(listPanel, 1);
    s.recipeDetail = new RecipeDetailsWidget(page);
    s.recipeDetail->setObjectName("recipeInlineDetail");
    columns->addWidget(s.recipeDetail, 3);
    layout->addLayout(columns, 1);
    const auto all = s.recipes->getAllRecipes();
    if (succeeded(all.outcome))
        for (const auto &recipe : all.value) {
            auto *item = new QListWidgetItem(
                QStringLiteral("%1\n%2 分钟 · 约 %3 千卡").arg(text(recipe.name)).arg(recipe.cookingTimeMinutes)
                    .arg(calculateNutrition(recipe, 1).calories, 0, 'f', 0),
                recipes);
            item->setIcon(Shixu::Ui::icon(Shixu::Ui::foodIcon(text(recipe.category))));
            item->setToolTip(item->text());
            item->setData(Qt::UserRole, recipe.id);
        }
    connect(recipes, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) loadRecipeDetails(m_services->recipeDetail, item->data(Qt::UserRole).toInt());
    });
    connect(s.recipeDetail, &RecipeDetailsWidget::cookingRequested, this, [this](int id) {
        openCooking(id);
        if (m_services->catalog->currentItem())
            loadRecipeDetails(m_services->recipeDetail, m_services->catalog->currentItem()->data(Qt::UserRole).toInt());
    });
    setPage(Page::Recipes, page);
    if (recipes->count()) recipes->setCurrentRow(0);
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
    if (m_services && m_services->statistics)
        m_services->statistics->refresh();
    if (!m_services)
        return;
    auto pending = m_services->cooking->getPendingCompletions();
    m_services->pending->clear();
    m_services->pendingArea->setVisible(!succeeded(pending.outcome) || !pending.value.empty());
    auto *nav = m_navigationButtons.at(static_cast<int>(Page::Recipes));
    nav->setText(succeeded(pending.outcome)
                     ? (pending.value.empty() ? QStringLiteral("菜谱与营养")
                                              : QStringLiteral("菜谱与营养 · %1").arg(pending.value.size()))
                     : QStringLiteral("菜谱与营养 · !"));
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

bool MainWindow::loadRecipeDetails(RecipeDetailsWidget *widget, int recipeId) {
    widget->clearRecipe();
    auto recipe = m_services->recipes->findRecipeById(recipeId);
    auto stock = m_services->inventory->getInventoryBatches();
    if (!succeeded(recipe.outcome) || !succeeded(stock.outcome)) {
        QMessageBox::warning(
            this, QStringLiteral("无法加载菜谱"),
            text(!succeeded(recipe.outcome) ? recipe.outcome.message : stock.outcome.message));
        return false;
    }
    V::EvaluationRequest request;
    request.inventory = stock.value;
    request.referenceDate = appNow().date().toString(Qt::ISODate).toStdString();
    for (const auto &i : recipe.value.ingredients)
        request.requirements.push_back({i.foodName, i.amountGrams});
    auto available = m_services->inventory->evaluateRequirements(request);
    if (!succeeded(available.outcome)) {
        QMessageBox::warning(this, QStringLiteral("库存读取失败"), text(available.outcome.message));
        return false;
    }
    widget->setRecipe(recipe.value, calculateNutrition(recipe.value, 1), available.value);
    return true;
}

void MainWindow::openRecipe(int recipeId) {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("菜谱详情"));
    dialog.resize(700, 650);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *widget = new RecipeDetailsWidget(&dialog);
    layout->addWidget(widget);
    if (!loadRecipeDetails(widget, recipeId)) return;
    if (m_showRecipeSteps)
        QTimer::singleShot(0, widget, [widget] { widget->showSteps(); });
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
        const auto recipe = m_services->recipes->findRecipeById(recipeId);
        if (!succeeded(recipe.outcome)) {
            QMessageBox::warning(this, QStringLiteral("无法加载菜谱"), text(recipe.outcome.message));
            return;
        }
        RecognitionService normalizer;
        for (const auto &ingredient : recipe.value.ingredients) {
            const auto name = normalizer.normalizeFoodName(text(ingredient.foodName)).toUtf8().toStdString();
            if (ingredient.amountGrams > 0 && Shixu::isPantryStaple(name) &&
                std::find(request.externallyPreparedFoods.begin(), request.externallyPreparedFoods.end(), name) == request.externallyPreparedFoods.end())
                request.externallyPreparedFoods.push_back(name);
        }
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("做饭确认与恢复"));
    dialog.setObjectName(QStringLiteral("cookingConfirmationDialog"));
    dialog.setStyleSheet(QStringLiteral("QDialog#cookingConfirmationDialog { background: #EDF7F9; }"));
    dialog.resize(680, 480);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *widget = new CookingConfirmationWidget(&dialog);
    layout->addWidget(widget);
    connect(widget, &CookingConfirmationWidget::closeRequested, &dialog, &QDialog::accept);
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
    if (eventId.isEmpty()) {
        const auto first = m_services->cooking->previewCompletion(request);
        widget->setPreview(first);
        if (first.outcome.status == V::OperationStatus::Succeeded && !first.confirmationToken.empty()) {
            widget->setBusy(true);
            progress(m_services->cooking->completeCooking(request, first.confirmationToken));
        }
    } else {
        V::CookingCompletionPreview display;
        display.snapshot = stored.snapshot;
        display.outcome = stored.progress.outcome;
        widget->setPreview(display);
        widget->setProgress(stored.progress);
    }
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
    if(page==Page::CheckIn)page=Page::Statistics;
    if (page == Page::Statistics && m_services && m_services->statistics)
        m_services->statistics->refresh();
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
    const int margin = width() < 1100 ? 24 : 32;
    m_content->layout()->setContentsMargins(home ? 0 : margin, home ? 0 : 24, home ? 0 : margin, home ? 0 : 24);
    m_content->layout()->setSpacing(home ? 0 : 16);
    m_content->setStyleSheet(QStringLiteral("QFrame#content { background: #EDF7F9; }"));
    m_pages->setStyleSheet(QStringLiteral("QStackedWidget#pageStack { background:transparent; border:0; }"));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!m_content || !m_pages) return;
    const bool home = m_pages->currentIndex() == int(Page::Home);
    const int margin = width() < 1100 ? 24 : 32;
    m_content->layout()->setContentsMargins(home ? 0 : margin, home ? 0 : 24,
                                           home ? 0 : margin, home ? 0 : 24);
}
