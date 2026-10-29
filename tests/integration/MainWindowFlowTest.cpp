#include "contracts/ServiceFactories.h"
#include "model/FoodCandidate.h"
#include "reward/RewardSystem.h"
#include "ui/CookingConfirmationWidget.h"
#include "ui/HomeWidget.h"
#include "ui/MainWindow.h"
#include "ui/RecipeDetailsWidget.h"
#include "ui/RecommendationWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
namespace V = Shixu::Contracts::V1;
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir(QDir::currentPath() + "/ui-flow-XXXXXX");
    if (!dir.isValid())
        return 1;
    const auto cwd = QDir::currentPath();
    QDir::setCurrent(dir.path());
    int outcome = 1;
    {
        MainWindow window(nullptr, dir.filePath("reward.ini"));
        window.show();
        auto *home = window.findChild<HomeWidget *>();
        auto *recommendations = window.findChild<RecommendationWidget *>();
        if (!home || !recommendations)
            return 2;
        // Deliberately empty stock exercises external preparation, including every
        // checkbox.
        home->selectMoodV1(V::Mood::WantGoodFood);
        home->findChild<QPushButton *>("homeMoodAction")->click();
        auto cards = recommendations->findChildren<QPushButton *>();
        QPushButton *details = nullptr;
        for (auto *b : cards)
            if (b->text() == QStringLiteral("查看菜谱详情")) {
                details = b;
                break;
            }
        if (!details)
            return 3;
        int phase = 0, ticks = 0;
        QTimer driver;
        driver.setInterval(20);
        QObject::connect(&driver, &QTimer::timeout, &window, [&] {
            if (++ticks > 300) {
                for (auto *d : window.findChildren<QDialog *>())
                    d->reject();
                driver.stop();
                return;
            }
            if (phase == 0) {
                auto widgets = window.findChildren<RecipeDetailsWidget *>();
                if (!widgets.isEmpty()) {
                    phase = 1;
                    widgets.first()->findChild<QPushButton *>("recipeComplete")->click();
                    widgets.first()->findChild<QPushButton *>("recipeComplete")->click();
                }
                return;
            }
            auto widgets = window.findChildren<CookingConfirmationWidget *>();
            if (widgets.isEmpty())
                return;
            auto *confirm = widgets.first();
            if (phase == 1) {
                for (auto *c : confirm->findChildren<QCheckBox *>())
                    if (c->isEnabled() && !c->isChecked()) {
                        c->click();
                        return;
                    }
                for (auto *b : confirm->findChildren<QPushButton *>())
                    if (b->text() == QStringLiteral("确认完成并记录") && b->isEnabled()) {
                        phase = 2;
                        b->click();
                        return;
                    }
            }
            if (phase == 2) {
                for (auto *label : confirm->findChildren<QLabel *>())
                    if (label->text().startsWith(QStringLiteral("已完成："))) {
                        outcome = 0;
                        phase = 3;
                        driver.stop();
                        qobject_cast<QDialog *>(confirm->parentWidget())->accept();
                        return;
                    }
            }
        });
        driver.start();
        details->click();
        driver.stop();
        if (outcome == 0) {
            auto recipes = V::createRecipeService(dir.filePath("meal_records.txt").toStdString());
            QSqlQuery q(QSqlDatabase::database("ShixuInventoryConnection"));
            if (!q.exec("SELECT count(*) FROM shixu_consumption_events") || !q.next() ||
                q.value(0).toInt() != 1)
                outcome = 4;
            auto *reward = window.findChild<RewardSystem *>();
            if (!reward || reward->totalCookingCount() != 1)
                outcome = 5;
            if (window.findChild<QLabel *>("verifiedCookingCount")->text() != QStringLiteral("1 次"))
                outcome = 7;
        }
    }
    // New window reads the same files without applying the completed event again.
    if (outcome == 0) {
        MainWindow restored(nullptr, dir.filePath("reward.ini"));
        auto *reward = restored.findChild<RewardSystem *>();
        if (!reward || reward->totalCookingCount() != 1)
            outcome = 6;
        if (restored.findChild<QLabel *>("verifiedCookingCount")->text() != QStringLiteral("1 次") ||
            !restored.findChild<QLabel *>("cookingAchievements")->text().contains(QStringLiteral("已解锁")))
            outcome = 8;
    }
    QDir::setCurrent(cwd);
    std::cout << "MainWindow empty-stock -> five-mood -> detail -> confirm -> restart: " << outcome << '\n';
    return outcome;
}
