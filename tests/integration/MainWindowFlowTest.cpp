#include <QFontDatabase>
#include <QFont>
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
#include <QListWidget>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
namespace V = Shixu::Contracts::V1;
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (QGuiApplication::platformName() == "offscreen") {
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/seguiemj.ttf");
    }
    const QString captures = qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) QDir().mkpath(captures);
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
            if (b->objectName() == QStringLiteral("viewRecommendedRecipe")) {
                details = b;
                break;
            }
        if (!details)
            return 3;
        int phase = 1, ticks = 0;
        QTimer driver;
        driver.setInterval(20);
        QObject::connect(&driver, &QTimer::timeout, &window, [&] {
            if (++ticks > 300) {
                for (auto *d : window.findChildren<QDialog *>())
                    d->reject();
                driver.stop();
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
                        if (!captures.isEmpty()) confirm->parentWidget()->grab().save(captures + "/cooking-confirmation.png");
                        b->click();
                        return;
                    }
            }
            if (phase == 2) {
                for (auto *label : confirm->findChildren<QLabel *>())
                    if (label->text().startsWith(QStringLiteral("已完成："))) {
                        outcome = 0;
                        if (!captures.isEmpty()) confirm->parentWidget()->grab().save(captures + "/cooking-completed.png");
                        phase = 3;
                        driver.stop();
                        qobject_cast<QDialog *>(confirm->parentWidget())->accept();
                        return;
                    }
            }
        });
        details->click();
        auto *detail = recommendations->findChild<RecipeDetailsWidget *>("recommendationRecipeDetails");
        auto *stages = recommendations->findChild<QStackedWidget *>("recommendationStages");
        if (!detail || !stages || stages->currentIndex() != 2 ||
            detail->findChild<QLabel *>("recipeTitle")->text().isEmpty()) return 13;
        recommendations->findChild<QPushButton *>("backToRecommendations")->click();
        if (stages->currentIndex() != 1 || !recommendations->findChild<QPushButton *>("recommendationExplanation")) return 14;
        details->click();
        driver.start();
        detail->findChild<QPushButton *>("recipeComplete")->click();
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
    if (outcome == 0) {
        QTemporaryDir directDir(cwd + "/ui-direct-XXXXXX");
        if (!directDir.isValid() || !QDir::setCurrent(directDir.path())) return 9;
        MainWindow direct(nullptr, directDir.filePath("reward.ini"));
        FoodCandidate tomato; tomato.name = QStringLiteral("番茄"); tomato.unit = "g"; tomato.quantity = 500;
        tomato.category = QStringLiteral("其他"); direct.addFood(tomato);
        FoodCandidate egg = tomato; egg.name = QStringLiteral("鸡蛋"); direct.addFood(egg);
        direct.showPage(MainWindow::Page::Recipes);
        auto *catalog = direct.findChild<QListWidget *>("recipeCatalog");
        auto *inlineDetail = direct.findChild<RecipeDetailsWidget *>("recipeInlineDetail");
        if (!catalog || !inlineDetail) return 10;
        for (int i = 0; i < catalog->count(); ++i)
            if (catalog->item(i)->data(Qt::UserRole).toInt() == 20) catalog->setCurrentRow(i);
        if (inlineDetail->findChild<QLabel *>("recipeTitle")->text() != QStringLiteral("番茄蛋花汤")) return 11;
        bool completed = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *title = dialog->findChild<QLabel *>("confirmTitle");
            completed = title && title->text().contains(QStringLiteral("做饭完成")) &&
                        title->text().contains(QStringLiteral("184 千卡"));
            dialog->accept();
        });
        inlineDetail->findChild<QPushButton *>("recipeComplete")->click();
        if (!completed || direct.findChild<QLabel *>("verifiedCookingCount")->text() != QStringLiteral("1 次") ||
            direct.findChild<QLabel *>("todayMealCount")->text() != QStringLiteral("1 次") ||
            direct.findChild<QLabel *>("todayCalories")->text() != QStringLiteral("184 千卡"))
            outcome = 12;
    }
    QDir::setCurrent(cwd);
    std::cout << "MainWindow empty-stock -> five-mood -> detail -> confirm -> restart: " << outcome << '\n';
    return outcome;
}
