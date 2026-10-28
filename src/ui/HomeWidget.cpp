#include "ui/HomeWidget.h"

#include "reward/RewardSystem.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QMetaMethod>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <vector>

namespace {

// Temporary bridge while MainWindow still listens to the three-choice signals.
Shixu::RecommendationEngine::Mood legacyMood(Shixu::Contracts::V1::Mood mood)
{
    using New = Shixu::Contracts::V1::Mood;
    using Old = Shixu::RecommendationEngine::Mood;
    switch (mood) {
    case New::Tired: return Old::Tired;
    case New::WantGoodFood:
    case New::TreatMyself: return Old::ReadyToCook;
    case New::LowAppetite:
    case New::WantLight: return Old::Neutral;
    default: return Old::NotSelected;
    }
}

struct FoodArt {
    int sprite;
    int shelf;
};

FoodArt artForFood(const QString& name)
{
    const QString food = name.toLower();
    if (food.contains(QStringLiteral("西兰花")) || food.contains(QStringLiteral("花椰菜"))
        || food.contains(QStringLiteral("broccoli"))) return {1, 2};
    if (food.contains(QStringLiteral("酸奶")) || food.contains(QStringLiteral("yogurt"))) return {6, 4};
    if (food.contains(QStringLiteral("奶酪")) || food.contains(QStringLiteral("芝士"))
        || food.contains(QStringLiteral("cheese"))) return {9, 0};
    if (food.contains(QStringLiteral("牛奶")) || food.contains(QStringLiteral("鲜奶"))
        || food.contains(QStringLiteral("milk"))) return {2, 4};
    if (food.contains(QStringLiteral("鸡蛋")) || food.contains(QStringLiteral("鸭蛋"))
        || food.contains(QStringLiteral("egg"))) return {3, 3};
    if (food.contains(QStringLiteral("苹果")) || food.contains(QStringLiteral("apple"))) return {0, 1};
    if (food.contains(QStringLiteral("胡萝卜")) || food.contains(QStringLiteral("carrot"))) return {4, 2};
    if (food.contains(QStringLiteral("番茄")) || food.contains(QStringLiteral("西红柿"))
        || food.contains(QStringLiteral("tomato"))) return {5, 1};
    if (food.contains(QStringLiteral("生菜")) || food.contains(QStringLiteral("菠菜"))
        || food.contains(QStringLiteral("白菜")) || food.contains(QStringLiteral("青菜"))
        || food.contains(QStringLiteral("lettuce"))) return {7, 2};
    if (food.contains(QStringLiteral("面包")) || food.contains(QStringLiteral("吐司"))
        || food.contains(QStringLiteral("bread"))) return {8, 0};
    if (food.contains(QStringLiteral("果汁")) || food.contains(QStringLiteral("橙汁"))
        || food.contains(QStringLiteral("饮料")) || food.contains(QStringLiteral("juice"))) return {11, 6};
    if (food.contains(QStringLiteral("果酱")) || food.contains(QStringLiteral("罐头"))
        || food.contains(QStringLiteral("酱")) || food.contains(QStringLiteral("jar"))) return {10, 5};
    return {-1, 0}; // Unrecognized foods use a neutral storage box.
}

QString quantityText(const HomeFoodItem& food)
{
    return QStringLiteral("%1%2").arg(QString::number(food.quantity, 'f', 1).remove(QRegularExpression(QStringLiteral("\\.0$"))), food.unit);
}

QPixmap foodIconPixmap(const QString& name)
{
    static const QPixmap sprites(QStringLiteral(":/home/food-sprites-morning.png"));
    static const QPixmap container(QStringLiteral(":/home/food-container-morning.png"));
    QPixmap icon(48, 48);
    icon.fill(Qt::transparent);
    const int index = artForFood(name).sprite;
    const qreal cellWidth = sprites.width() / 4.0;
    const qreal cellHeight = sprites.height() / 3.0;
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (index < 0) {
        if (!container.isNull()) painter.drawPixmap(QRectF(0, 0, 48, 48), container, QRectF(container.rect()));
    } else if (!sprites.isNull()) {
        painter.drawPixmap(QRectF(0, 0, 48, 48), sprites,
                           QRectF((index % 4) * cellWidth, (index / 4) * cellHeight,
                                  cellWidth, cellHeight));
    }
    return icon;
}

QFrame* card(QWidget* parent, const QString& title, QVBoxLayout*& layout)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("homeCard"));
    layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);
    auto* heading = new QLabel(title, frame);
    heading->setObjectName(QStringLiteral("homeCardTitle"));
    layout->addWidget(heading);
    return frame;
}

} // namespace

class FridgeCanvas : public QWidget
{
public:
    explicit FridgeCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_closed(QStringLiteral(":/home/fridge-closed-morning.png"))
        , m_open(QStringLiteral(":/home/fridge-open-morning.png"))
        , m_foodSprites(QStringLiteral(":/home/food-sprites-morning.png"))
        , m_genericContainer(QStringLiteral(":/home/food-container-morning.png"))
    {
        setMinimumSize(260, 380);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击冰箱开门或关门"));
    }

    void setFoods(const QList<HomeFoodItem>& foods)
    {
        m_foods = foods;
        update();
    }

    bool isOpen() const { return m_isOpen; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QPixmap& pixmap = m_isOpen ? m_open : m_closed;
        if (pixmap.isNull()) {
            return;
        }
        const QSizeF fitted = QSizeF(pixmap.size()).scaled(size(), Qt::KeepAspectRatio);
        const QRectF imageRect((width() - fitted.width()) / 2, (height() - fitted.height()) / 2,
                               fitted.width(), fitted.height());
        painter.drawPixmap(imageRect, pixmap, QRectF(pixmap.rect()));
        if (!m_isOpen || m_foodSprites.isNull()) {
            return;
        }

        // The first three shelves are inside the fridge; the others are on the door.
        constexpr std::array<qreal, 7> shelfBottom{{.238, .333, .432, .243, .355, .505, .658}};
        std::array<std::vector<int>, 7> shelfFoods;
        const int visible = std::min(static_cast<int>(m_foods.size()), 14);
        for (int i = 0; i < visible; ++i) {
            const int preferred = artForFood(m_foods[i].name).shelf;
            for (int offset = 0; offset < 7; ++offset) {
                const int shelf = (preferred + offset) % 7;
                if (shelfFoods[shelf].size() < 2) {
                    shelfFoods[shelf].push_back(i);
                    break;
                }
            }
        }

        const qreal cellWidth = m_foodSprites.width() / 4.0;
        const qreal cellHeight = m_foodSprites.height() / 3.0;
        for (int shelf = 0; shelf < 7; ++shelf) {
            const bool door = shelf >= 3;
            const auto& foods = shelfFoods[shelf];
            for (size_t position = 0; position < foods.size(); ++position) {
                const FoodArt art = artForFood(m_foods[foods[position]].name);
                const qreal centerX = foods.size() == 1 ? (door ? .72 : .37)
                    : (door ? (position == 0 ? .66 : .79) : (position == 0 ? .30 : .44));
                const qreal side = imageRect.width() * (door ? .175 : .185);
                const QRectF target(imageRect.x() + centerX * imageRect.width() - side / 2,
                                    imageRect.y() + shelfBottom[shelf] * imageRect.height() - side,
                                    side, side);
                if (art.sprite < 0) {
                    if (!m_genericContainer.isNull())
                        painter.drawPixmap(target, m_genericContainer, QRectF(m_genericContainer.rect()));
                } else {
                    const QRectF source((art.sprite % 4) * cellWidth, (art.sprite / 4) * cellHeight,
                                        cellWidth, cellHeight);
                    painter.drawPixmap(target, m_foodSprites, source);
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_isOpen = !m_isOpen;
            update();
        }
        QWidget::mousePressEvent(event);
    }

private:
    QPixmap m_closed;
    QPixmap m_open;
    QPixmap m_foodSprites;
    QPixmap m_genericContainer;
    QList<HomeFoodItem> m_foods;
    bool m_isOpen = false;
};

HomeWidget::HomeWidget(RewardSystem* rewards, QWidget* parent)
    : QWidget(parent)
    , m_rewards(rewards)
{
    setObjectName(QStringLiteral("homeWidget"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto* hero = new QVBoxLayout;
    hero->setSpacing(3);
    m_fridge = new FridgeCanvas(this);
    hero->addWidget(m_fridge, 1);
    auto* tip = new QLabel(QStringLiteral("轻点冰箱门，看看里面有什么  ·  再点一次关门"), this);
    tip->setObjectName(QStringLiteral("homeTip"));
    tip->setAlignment(Qt::AlignCenter);
    hero->addWidget(tip);
    root->addLayout(hero, 1);

    auto* right = new QVBoxLayout;
    right->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("打开冰箱，\n看看今天有什么"), this);
    title->setObjectName(QStringLiteral("homeTitle"));
    right->addWidget(title);
    auto* subtitle = new QLabel(QStringLiteral("记录食材  ·  合理规划  ·  减少浪费"), this);
    subtitle->setObjectName(QStringLiteral("homeSubtitle"));
    right->addWidget(subtitle);
    right->addSpacing(8);

    auto* sideScroll = new QScrollArea(this);
    sideScroll->setObjectName(QStringLiteral("homeSideScroll"));
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setFixedWidth(340);
    auto* side = new QWidget(sideScroll);
    side->setObjectName(QStringLiteral("homeSide"));
    side->setMaximumWidth(326);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(10);
    sideScroll->setWidget(side);

    QVBoxLayout* moodLayout;
    sideLayout->addWidget(card(side, QStringLiteral("今天的心情"), moodLayout));
    auto* moodDescription = new QLabel(
        QStringLiteral("选一个状态，看看今天适合做什么。"), side);
    moodDescription->setObjectName(QStringLiteral("homeBody"));
    moodDescription->setWordWrap(true);
    moodLayout->addWidget(moodDescription);

    m_moodButtons = new QButtonGroup(this);
    m_moodButtons->setExclusive(true);
    using V1Mood = Shixu::Contracts::V1::Mood;
    const struct { V1Mood mood; const char* label; int minutes; } moods[] = {
        {V1Mood::Tired, "很累", 30},
        {V1Mood::WantGoodFood, "想吃点好的", 45},
        {V1Mood::LowAppetite, "没胃口", 30},
        {V1Mood::WantLight, "想清爽一点", 40},
        {V1Mood::TreatMyself, "想犒劳自己", 75},
    };
    for (const auto& choice : moods) {
        auto* button = new QPushButton(
            QStringLiteral("%1　·　默认 %2 分钟")
                .arg(QString::fromUtf8(choice.label)).arg(choice.minutes), side);
        button->setObjectName(QStringLiteral("homeMoodChoice"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(38);
        m_moodButtons->addButton(button, static_cast<int>(choice.mood));
        moodLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, choice] {
            m_moodHint->hide();
            if (!isSignalConnected(QMetaMethod::fromSignal(&HomeWidget::moodSelectedV1)))
                emit moodSelected(legacyMood(choice.mood));
            emit moodSelectedV1(choice.mood);
        });
    }

    m_moodHint = new QLabel(QStringLiteral("先选一个心情吧。"), side);
    m_moodHint->setObjectName(QStringLiteral("homeMoodHint"));
    m_moodHint->hide();
    moodLayout->addWidget(m_moodHint);
    auto* moodAction = new QPushButton(QStringLiteral("看看今天吃什么  →"), side);
    moodAction->setObjectName(QStringLiteral("homeMoodAction"));
    moodAction->setCursor(Qt::PointingHandCursor);
    moodAction->setMinimumHeight(42);
    moodLayout->addWidget(moodAction);
    connect(moodAction, &QPushButton::clicked, this, [this] {
        const int selected = m_moodButtons->checkedId();
        if (selected < 0) {
            m_moodHint->show();
            return;
        }
        const auto mood = static_cast<Shixu::Contracts::V1::Mood>(selected);
        if (!isSignalConnected(QMetaMethod::fromSignal(
                &HomeWidget::moodRecommendationRequestedV1)))
            emit moodRecommendationRequested(legacyMood(mood));
        emit moodRecommendationRequestedV1(mood);
    });

    QVBoxLayout* inventoryLayout;
    sideLayout->addWidget(card(side, QStringLiteral("冰箱概览"), inventoryLayout));
    m_inventoryCount = new QLabel(side);
    m_inventoryCount->setObjectName(QStringLiteral("homeMetric"));
    inventoryLayout->addWidget(m_inventoryCount);
    m_inventoryPreview = new QLabel(side);
    m_inventoryPreview->setObjectName(QStringLiteral("homeBody"));
    m_inventoryPreview->setWordWrap(true);
    inventoryLayout->addWidget(m_inventoryPreview);
    m_foodGrid = new QGridLayout;
    m_foodGrid->setSpacing(8);
    inventoryLayout->addLayout(m_foodGrid);
    auto* inventoryButton = new QPushButton(QStringLiteral("查看全部食材  →"), side);
    inventoryButton->setObjectName(QStringLiteral("homeLink"));
    inventoryLayout->addWidget(inventoryButton);
    connect(inventoryButton, &QPushButton::clicked, this, &HomeWidget::inventoryRequested);

    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(9);
    QVBoxLayout* expiryLayout;
    statusRow->addWidget(card(side, QStringLiteral("临期提醒"), expiryLayout), 1);
    expiryLayout->setContentsMargins(13, 14, 13, 14);
    m_expiringCount = new QLabel(side);
    m_expiringCount->setObjectName(QStringLiteral("homeSmallMetric"));
    expiryLayout->addWidget(m_expiringCount);
    m_expiringPreview = new QLabel(side);
    m_expiringPreview->setObjectName(QStringLiteral("homeBody"));
    m_expiringPreview->setWordWrap(true);
    expiryLayout->addWidget(m_expiringPreview);

    QVBoxLayout* checkInLayout;
    statusRow->addWidget(card(side, QStringLiteral("今日打卡"), checkInLayout), 1);
    checkInLayout->setContentsMargins(13, 14, 13, 14);
    m_checkInStatus = new QLabel(side);
    m_checkInStatus->setObjectName(QStringLiteral("homeBody"));
    m_checkInStatus->setWordWrap(true);
    checkInLayout->addWidget(m_checkInStatus);
    auto* checkInButton = new QPushButton(QStringLiteral("前往打卡  →"), side);
    checkInButton->setObjectName(QStringLiteral("homeLink"));
    checkInLayout->addWidget(checkInButton);
    connect(checkInButton, &QPushButton::clicked, this, &HomeWidget::checkInRequested);
    sideLayout->addLayout(statusRow);

    auto* foodEntryButton = new QPushButton(QStringLiteral("＋  录入食材"), side);
    foodEntryButton->setObjectName(QStringLiteral("homeAction"));
    foodEntryButton->setMinimumHeight(46);
    sideLayout->addWidget(foodEntryButton);
    connect(foodEntryButton, &QPushButton::clicked, this, &HomeWidget::foodEntryRequested);
    auto* recipesButton = new QPushButton(QStringLiteral("看看菜谱  →"), side);
    recipesButton->setObjectName(QStringLiteral("homeLink"));
    sideLayout->addWidget(recipesButton);
    connect(recipesButton, &QPushButton::clicked, this, &HomeWidget::recipesRequested);
    sideLayout->addStretch();
    right->addWidget(sideScroll, 1);
    root->addLayout(right);

    setStyleSheet(QStringLiteral(R"(
        QWidget#homeWidget, QWidget#homeSide, QScrollArea#homeSideScroll {
            background: transparent; color: #173956;
        }
        QLabel#homeTitle { color: #143B67; font-size: 27px; font-weight: 700; }
        QLabel#homeSubtitle, QLabel#homeTip { color: #66849A; font-size: 12px; }
        QFrame#homeCard { background: rgba(255, 255, 255, 237); border: 1px solid #E2EDF0; border-radius: 18px; }
        QLabel#homeCardTitle { color: #18446E; font-size: 16px; font-weight: 700; }
        QLabel#homeMetric { color: #2475AA; font-size: 22px; font-weight: 700; }
        QLabel#homeSmallMetric { color: #E38D45; font-size: 18px; font-weight: 700; }
        QLabel#homeBody { color: #6C8493; font-size: 12px; }
        QFrame#homeFoodTile { background: #F5FAFB; border: 1px solid #E4EFF0; border-radius: 11px; }
        QLabel#homeFoodName { color: #294F66; font-size: 11px; font-weight: 700; }
        QPushButton#homeMoodChoice {
            color: #31566D; background: #F0F7FA; text-align: left;
            border: 1px solid #D6E7EF; border-radius: 9px;
            padding: 7px 12px; font-size: 13px;
        }
        QPushButton#homeMoodChoice:hover { background: #E8F3F8; }
        QPushButton#homeMoodChoice:checked {
            color: #185D88; background: #DCEEF8;
            border: 2px solid #62A9D3; font-weight: 700;
        }
        QLabel#homeMoodHint { color: #C05239; font-size: 12px; }
        QPushButton#homeMoodAction {
            color: white; background: #358FCA; border: 0;
            border-radius: 9px; font-size: 13px; font-weight: 700;
        }
        QPushButton#homeMoodAction:hover { background: #267DB6; }
        QPushButton#homeLink { color: #3684BA; background: transparent; border: 0; text-align: left; padding: 4px 0; font-weight: 600; }
        QPushButton#homeLink:hover { color: #174E83; }
        QPushButton#homeAction { color: #FFFFFF; background: #3C9AD9; border: 0; border-radius: 12px; padding: 11px; font-weight: 700; }
        QPushButton#homeAction:hover { background: #237EBB; }
    )"));

    connect(m_rewards, &RewardSystem::changed, this, &HomeWidget::refresh);
    refresh();
}

void HomeWidget::selectMood(Shixu::RecommendationEngine::Mood mood)
{
    using Old = Shixu::RecommendationEngine::Mood;
    using New = Shixu::Contracts::V1::Mood;
    switch (mood) {
    case Old::ReadyToCook: selectMoodV1(New::WantGoodFood); break;
    case Old::Neutral: selectMoodV1(New::LowAppetite); break;
    case Old::Tired: selectMoodV1(New::Tired); break;
    default: break;
    }
}
void HomeWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    static const QPixmap background(QStringLiteral(":/home/morning-background.png"));
    if (!background.isNull()) {
        painter.drawPixmap(rect(), background, background.rect());
    }
    QWidget::paintEvent(event);
}

void HomeWidget::setInventory(const QList<HomeFoodItem>& foods)
{
    m_foods = foods;
    m_inventoryConnected = true;
    m_fridge->setFoods(foods);
    refresh();
}

void HomeWidget::refresh()
{
    while (auto* item = m_foodGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_inventoryConnected) {
        m_inventoryCount->setText(QStringLiteral("等待库存接入"));
        m_inventoryPreview->setText(QStringLiteral("连接库存后，在这里查看食材。"));
        m_expiringCount->setText(QStringLiteral("—"));
        m_expiringPreview->setText(QStringLiteral("接入库存后自动显示到期提醒。"));
    } else {
        m_inventoryCount->setText(QStringLiteral("%1 项食材").arg(m_foods.size()));
        m_inventoryPreview->setText(m_foods.isEmpty()
            ? QStringLiteral("冰箱还空着，先录入一些食材吧。")
            : QStringLiteral("轻点冰箱门，看看食材放在哪里。"));
        for (int i = 0; i < std::min(static_cast<int>(m_foods.size()), 4); ++i) {
            const HomeFoodItem& food = m_foods[i];
            auto* tile = new QFrame(this);
            tile->setObjectName(QStringLiteral("homeFoodTile"));
            auto* tileLayout = new QVBoxLayout(tile);
            tileLayout->setContentsMargins(4, 5, 4, 5);
            tileLayout->setSpacing(2);
            auto* icon = new QLabel(tile);
            icon->setAlignment(Qt::AlignCenter);
            icon->setPixmap(foodIconPixmap(food.name));
            tileLayout->addWidget(icon);
            auto* name = new QLabel(food.name, tile);
            name->setObjectName(QStringLiteral("homeFoodName"));
            name->setAlignment(Qt::AlignCenter);
            name->setToolTip(QStringLiteral("%1  %2").arg(food.name, quantityText(food)));
            tileLayout->addWidget(name);
            m_foodGrid->addWidget(tile, i / 4, i % 4);
        }

        const QDate today = QDate::currentDate();
        QStringList expiring;
        for (const HomeFoodItem& food : m_foods) {
            if (food.expiryDate.isValid() && today.daysTo(food.expiryDate) <= 3) {
                const int days = today.daysTo(food.expiryDate);
                const QString when = days < 0 ? QStringLiteral("已过期")
                    : days == 0 ? QStringLiteral("今天到期") : QStringLiteral("%1 天后到期").arg(days);
                expiring << QStringLiteral("%1  ·  %2").arg(food.name, when);
            }
        }
        m_expiringCount->setText(QStringLiteral("%1 项需留意").arg(expiring.size()));
        m_expiringPreview->setText(expiring.isEmpty() ? QStringLiteral("近 3 天没有临期食材。")
                                                     : expiring.mid(0, 2).join(QLatin1Char('\n')));
    }

    m_checkInStatus->setText(m_rewards->hasCheckedIn(QDate::currentDate())
        ? QStringLiteral("今天已打卡 · 连续 %1 天 · %2 积分").arg(m_rewards->currentStreak()).arg(m_rewards->points())
        : QStringLiteral("今天还没有打卡 · 当前 %1 积分").arg(m_rewards->points()));
}

void HomeWidget::selectMoodV1(Shixu::Contracts::V1::Mood mood)
{
    if (auto* button = qobject_cast<QPushButton*>(m_moodButtons->button(static_cast<int>(mood)))) {
        button->setChecked(true);
        m_moodHint->hide();
    }
}
