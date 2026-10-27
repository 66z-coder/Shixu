#include "recommendation/RecommendationContextBuilder.h"
#include "RecommendationText.h"
#include "inventory.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>

using namespace Shixu::RecommendationEngine;

namespace {
QStringList parseTags(QString text)
{
    text.replace(QStringLiteral("，"), QStringLiteral(","));
    QStringList values;
    for (const QString& item : text.split(QLatin1Char(','))) {
        if (!item.trimmed().isEmpty()) {
            values.append(item.trimmed());
        }
    }
    values.removeDuplicates();
    return values;
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Shixu"));
    app.setApplicationName(QStringLiteral("RecommendationLiveDemo"));
    // B currently opens a relative shixu.db. Give this integration demo its own
    // persistent directory, without changing B's API or any existing database.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataDir.isEmpty() || !QDir().mkpath(dataDir) || !QDir::setCurrent(dataDir)) {
        QMessageBox::critical(nullptr, QStringLiteral("库存目录不可用"),
                              QStringLiteral("无法创建推荐联调窗口的库存目录。"));
        return 1;
    }
    InventoryManager inventory;
    const bool databaseOpen = [] {
        const QSqlDatabase database = QSqlDatabase::database(QStringLiteral("ShixuInventoryConnection"), false);
        return database.isOpen();
    }();
    if (!databaseOpen) {
        QMessageBox::critical(nullptr, QStringLiteral("库存打开失败"),
                              QStringLiteral("B 模块的 SQLite 库存未能打开，请查看运行输出。"));
        return 1;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("食序 - B/C/D 真实接口联调"));
    window.resize(1080, 780);
    auto* layout = new QVBoxLayout(&window);
    auto* intro = new QLabel(QStringLiteral(
        "输入的食材通过 B 保存到 SQLite，再由 C 的 30 道内置菜谱生成候选，D 负责评分、Top-K 和解释。\n"
        "本窗口使用独立联调库存；首次为空。示例可输入：番茄 300 g、鸡蛋 200 g、食用油 50 g。"));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* controls = new QHBoxLayout;
    auto* foodBox = new QGroupBox(QStringLiteral("添加联调库存"));
    auto* foodForm = new QFormLayout(foodBox);
    auto* name = new QLineEdit;
    name->setPlaceholderText(QStringLiteral("例如：番茄"));
    auto* quantity = new QDoubleSpinBox;
    quantity->setDecimals(3);
    quantity->setRange(0.001, 1000000.0);
    quantity->setValue(300.0);
    auto* unit = new QLineEdit(QStringLiteral("g"));
    unit->setPlaceholderText(QStringLiteral("g / kg / 克 / 个等"));
    auto* unknownExpiry = new QCheckBox(QStringLiteral("到期日未知"));
    unknownExpiry->setChecked(true);
    auto* expiry = new QDateEdit(QDate::currentDate().addDays(2));
    expiry->setCalendarPopup(true);
    expiry->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    expiry->setEnabled(false);
    auto* add = new QPushButton(QStringLiteral("保存到 B 的库存并刷新推荐"));
    foodForm->addRow(QStringLiteral("食材名称"), name);
    foodForm->addRow(QStringLiteral("数量"), quantity);
    foodForm->addRow(QStringLiteral("单位"), unit);
    foodForm->addRow(unknownExpiry);
    foodForm->addRow(QStringLiteral("到期日"), expiry);
    foodForm->addRow(add);
    controls->addWidget(foodBox);

    auto* settingsBox = new QGroupBox(QStringLiteral("D 的推荐条件"));
    auto* settingsForm = new QFormLayout(settingsBox);
    auto* k = new QSpinBox;
    k->setRange(1, 30);
    k->setValue(5);
    auto* servings = new QSpinBox;
    servings->setRange(1, 20);
    auto* minutes = new QSpinBox;
    minutes->setRange(0, 240);
    minutes->setValue(60);
    minutes->setSpecialValueText(QStringLiteral("不限时间"));
    auto* excluded = new QLineEdit;
    excluded->setPlaceholderText(QStringLiteral("例如：花生，虾"));
    auto* preferred = new QLineEdit;
    preferred->setPlaceholderText(QStringLiteral("例如：家常，清淡"));
    auto* calories = new QDoubleSpinBox;
    calories->setRange(0.0, 10000.0);
    calories->setSpecialValueText(QStringLiteral("未设置，不参与此项评分"));
    auto* refresh = new QPushButton(QStringLiteral("读取库存并重新推荐"));
    settingsForm->addRow(QStringLiteral("最多推荐几道"), k);
    settingsForm->addRow(QStringLiteral("份数"), servings);
    settingsForm->addRow(QStringLiteral("最长制作分钟"), minutes);
    settingsForm->addRow(QStringLiteral("忌口（逗号分隔）"), excluded);
    settingsForm->addRow(QStringLiteral("偏好标签"), preferred);
    settingsForm->addRow(QStringLiteral("每份能量目标 kcal"), calories);
    settingsForm->addRow(refresh);
    controls->addWidget(settingsBox);
    layout->addLayout(controls);

    auto* status = new QLabel;
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* splitter = new QSplitter;
    auto* stock = new QPlainTextEdit;
    stock->setReadOnly(true);
    auto* output = new QPlainTextEdit;
    output->setReadOnly(true);
    splitter->addWidget(stock);
    splitter->addWidget(output);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);
    auto* location = new QLabel(QStringLiteral("联调库存文件：%1").arg(QDir(dataDir).filePath(QStringLiteral("shixu.db"))));
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(location);

    const auto update = [&] {
        UserContext preferences;
        preferences.topK = k->value();
        preferences.servings = servings->value();
        preferences.maxPreparationMinutes = minutes->value();
        preferences.excludedIngredients = parseTags(excluded->text());
        preferences.preferredTags = parseTags(preferred->text());
        preferences.targetCaloriesPerServing = calories->value();
        const auto built = readUserContext(inventory, preferences);
        QStringList statusLines{QStringLiteral("可用批次 %1；已过期排除 %2；单位待换算 %3。")
                                   .arg(built.usableBatches).arg(built.expiredBatches).arg(built.unconvertedBatches)};
        statusLines.append(built.warnings);
        status->setText(statusLines.join(QLatin1Char('\n')));
        QStringList stockLines{QStringLiteral("B 的 SQLite 库存（含过期和待换算记录）：")};
        for (const FoodCandidate& food : inventory.getAllItems()) {
            stockLines << QStringLiteral("%1：%2 %3；到期 %4")
                              .arg(food.name).arg(food.quantity).arg(food.unit)
                              .arg(food.expiryDate.isValid() ? food.expiryDate.toString(Qt::ISODate)
                                                            : QStringLiteral("未知"));
        }
        stock->setPlainText(stockLines.join(QLatin1Char('\n')));
        output->setPlainText(recommendationText(recommend(built.context)));
    };
    QObject::connect(unknownExpiry, &QCheckBox::toggled, expiry,
                     [expiry](bool checked) { expiry->setEnabled(!checked); });
    QObject::connect(refresh, &QPushButton::clicked, &window, update);
    QObject::connect(add, &QPushButton::clicked, &window, [&] {
        if (name->text().trimmed().isEmpty() || unit->text().trimmed().isEmpty()) {
            QMessageBox::information(&window, QStringLiteral("请补充输入"), QStringLiteral("请填写食材名称和单位。"));
            return;
        }
        FoodCandidate food;
        food.name = name->text().trimmed();
        food.quantity = quantity->value();
        food.unit = unit->text().trimmed();
        food.expiryDate = unknownExpiry->isChecked() ? QDate() : expiry->date();
        const auto before = inventory.getAllItems().size();
        inventory.addFood(food);
        if (inventory.getAllItems().size() <= before) {
            QMessageBox::warning(&window, QStringLiteral("保存未完成"),
                                 QStringLiteral("B 模块未保存此记录，请查看数据库运行输出。"));
            return;
        }
        name->clear();
        update();
    });
    update();
    window.show();
    return app.exec();
}
