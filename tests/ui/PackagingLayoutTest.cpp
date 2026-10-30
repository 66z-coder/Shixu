#include "recognition/PackagingEntryWidget.h"
#include "ui/MainWindow.h"
#include "ui/AppTheme.h"
#include "ui/InventoryWidget.h"
#include "ui/RecommendationWidget.h"
#include "ui/RecipeDetailsWidget.h"
#include <QFrame>
#include "core/UserProfile.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QStackedWidget>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTimer>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    // The offscreen platform has no system font discovery on Windows.
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == "offscreen") {
        const int font =
            QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        check(font >= 0, "load Chinese font for offscreen rendering");
        app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/seguiemj.ttf");
    }
#endif
    Shixu::applyTheme();
    const QString output = qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
    const QString original = QDir::currentPath();
    QTemporaryDir data(QDir::currentPath() + "/ui-layout-XXXXXX");
    if (!data.isValid())
        return 1;
    QDir::setCurrent(data.path());
    int result = 0;
    try {
        {
            InventoryWidget inventory;
            FoodCandidate food;
            food.name = QStringLiteral("番茄"); food.unit = QStringLiteral("盒");
            food.category = QStringLiteral("其他"); food.quantity = 12.333333333333334;
            food.gramsPerUnit = 150; food.totalWeightGrams = 1850; food.batchId = 1;
            FoodCandidate integer = food; integer.batchId = 2; integer.quantity = 2;
            FoodCandidate partial = food; partial.batchId = 3; partial.quantity = 0.5;
            FoodCandidate unknown = food; unknown.batchId = 4; unknown.gramsPerUnit = 0;
            FoodCandidate weighed = food; weighed.batchId = 5; weighed.unit = "kg"; weighed.quantity = 1.25;
            inventory.setItems({food, integer, partial, unknown, weighed});
            auto *inventoryTable = inventory.findChild<QTableWidget *>("inventoryTable");
            check(inventoryTable->item(0, 2)->text() == QStringLiteral("约 12 盒当量 + 50 克"), "fractional count shown with remainder");
            check(inventoryTable->item(1, 2)->text() == QStringLiteral("2 盒"), "whole count unchanged");
            check(inventoryTable->item(2, 2)->text() == QStringLiteral("约 75 克"), "partial unit shown in grams");
            check(inventoryTable->item(3, 2)->text().contains(QStringLiteral("重量未知")), "unknown weight identified");
            check(inventoryTable->item(4, 2)->text() == QStringLiteral("1.25 kg"), "weight units retain decimals");
            FoodCandidate saved;
            inventory.setUpdateHandler([&](FoodCandidate value, QString *) { saved = value; return true; });
            inventoryTable->selectRow(0);
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (dialog) {
                app.processEvents();
                if (!output.isEmpty()) { QDir().mkpath(output); dialog->grab().save(output + "/inventory-edit-dialog.png"); }
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            }
            });
            inventory.findChild<QPushButton *>("inventoryEdit")->click();
            check(saved.batchId == 1 && saved.quantity == food.quantity && saved.totalWeightGrams == 1850,
                  "unchanged edit keeps precise original quantity and weight");
        }
        MainWindow window(nullptr, data.filePath("reward.ini"));
        window.showPage(MainWindow::Page::FoodEntry);
        window.show();
        auto *entry = window.findChild<PackagingEntryWidget *>();
        auto *scroll = entry->findChild<QScrollArea *>("entryScroll");
        auto *list = entry->findChild<QListWidget *>("pendingFoods");
        auto *submit = entry->findChild<QPushButton *>("submitCandidate");
        for (const QString &name : {QStringLiteral("番茄"), QStringLiteral("牛奶"),
                                    QStringLiteral("这是一条用于验证自动换行的较长食材名称")}) {
            entry->findChild<QLineEdit *>("foodName")->setText(name);
            entry->findChild<QComboBox *>("foodUnit")->setCurrentText("盒");
            entry->findChild<QComboBox *>("dateMode")->setCurrentIndex(3);
            entry->findChild<QCheckBox *>("confirmed")->setChecked(true);
            entry->findChild<QPushButton *>("saveFood")->click();
        }
        list->setCurrentRow(0);
        check(list->count() == 3 && submit->isEnabled(), "queued foods ready to submit");
        for (QSize size : {QSize(1280, 800), QSize(980, 620)}) {
            window.resize(size);
            scroll->verticalScrollBar()->setValue(0);
            for (int i = 0; i < 5; ++i)
                app.processEvents();
            check(window.size() == size, "window must fit requested size");
            check(scroll->horizontalScrollBar()->maximum() == 0, "editor must fit viewport width");
            check(list->horizontalScrollBar()->maximum() == 0, "long list text must wrap");
            check(scroll->verticalScrollBar()->maximum() > 0, "long form must scroll");
            const QRect submitRect(submit->mapTo(entry, QPoint()), submit->size());
            check(entry->rect().contains(submitRect), "submit button must stay visible");
            check(!entry->findChild<QDialog*>("packagingDateDialog")->isVisible(), "OCR is outside first screen");
            for (auto* field : {static_cast<QWidget*>(entry->findChild<QLineEdit*>("foodName")),
                                static_cast<QWidget*>(entry->findChild<QComboBox*>("foodCategory"))})
                check(scroll->viewport()->rect().contains(QRect(field->mapTo(scroll->viewport(), QPoint()), field->size())),
                      "name and category fully visible on first screen");
            for (auto *field : entry->findChildren<QComboBox *>()) {
                if (!field->isVisible()) continue;
                const QRect fieldRect(field->mapTo(scroll->widget(), QPoint()), field->size());
                check(fieldRect.right() < scroll->viewport()->width(),
                      "form field must not be clipped horizontally");
            }
            if (!output.isEmpty()) {
                QDir().mkpath(output);
                check(window.grab().save(output +
                                         QString("/entry-%1x%2.png").arg(size.width()).arg(size.height())),
                      "save screenshot");
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                app.processEvents();
                check(window.grab().save(
                          output + QString("/entry-form-%1x%2.png").arg(size.width()).arg(size.height())),
                      "save form screenshot");
            }
        }
        window.resize(1280, 800);
        entry->findChild<QLineEdit*>("foodName")->setText(QStringLiteral("牛奶"));
        entry->findChild<QComboBox*>("foodUnit")->setCurrentText(QStringLiteral("盒"));
        entry->findChild<QDoubleSpinBox*>("foodQuantity")->setValue(2);
        entry->findChild<QComboBox*>("netContentUnit")->setCurrentText("ml");
        entry->findChild<QDoubleSpinBox*>("gramsPerUnit")->setValue(250);
        entry->findChild<QComboBox*>("dateMode")->setCurrentIndex(1);
        entry->findChild<QLineEdit*>("expiryDate")->setText("2027-01-01");
        scroll->verticalScrollBar()->setValue(0);
        app.processEvents();
        if (!output.isEmpty()) check(window.grab().save(output + "/entry-volume-example.png"), "capture volume form");
        entry->findChild<QPushButton*>("openDateRecognition")->click();
        app.processEvents();
        auto* dateDialog = entry->findChild<QDialog*>("packagingDateDialog");
        check(dateDialog->isVisible(), "recognition dialog can open in main window");
        if (!output.isEmpty()) check(dateDialog->grab().save(output + "/packaging-date-dialog.png"), "capture date dialog");
        dateDialog->reject();
        entry->findChild<QLabel *>("entryStatus")
            ->setText(
                QStringLiteral(
                    "包装日期无法确定，请核对完整年份和日期。请重新选择清晰图片，或者手动输入日期信息。")
                    .repeated(3));
        app.processEvents();
        check(scroll->horizontalScrollBar()->maximum() == 0, "long status must wrap");
        check(!window.findChild<QPushButton *>("pendingCookingButton"), "no global pending strip");
        window.showPage(MainWindow::Page::CheckIn);auto* growthPage=window.findChild<QStackedWidget*>("pageStack")->currentWidget();window.showPage(MainWindow::Page::Statistics);check(window.findChild<QStackedWidget*>("pageStack")->currentWidget()==growthPage,"legacy routes share one growth page");
        int visibleGrowth=0;for(auto* nav:window.findChildren<QPushButton*>("navButton"))if(!nav->isHidden()&&nav->text()==QStringLiteral("我的成长"))++visibleGrowth;
        check(visibleGrowth==1,"single growth navigation");

        auto *recommendation = window.findChild<RecommendationWidget *>();
        window.showPage(MainWindow::Page::Recommendations);
        recommendation->selectMoodV1(Shixu::Contracts::V1::Mood::Tired);
        recommendation->requestRecommendations();
        window.resize(1600, 900);
        for (int i = 0; i < 5; ++i) app.processEvents();
        auto *resultScroll = recommendation->findChild<QScrollArea *>("recommendationResultsScroll");
        auto *board = recommendation->findChild<QWidget *>("recommendationBoard");
        check(board, "real recipe results available");
        const auto recommendationCards = board->findChildren<QFrame *>("recommendationCard");
        check(recommendationCards.size() == 3, "primary and two alternatives");
        auto *primaryCard = recommendationCards[0];
        auto *alternativeOne = recommendationCards[1];
        auto *alternativeTwo = recommendationCards[2];
        check(primaryCard->property("primaryCard").toBool(), "primary card first");
        check(primaryCard->x() < alternativeOne->x() && alternativeOne->x() == alternativeTwo->x() &&
              alternativeOne->y() < alternativeTwo->y(), "primary left and alternatives stacked right");
        const double share = double(primaryCard->width()) / (primaryCard->width() + alternativeOne->width());
        check(share >= 0.55 && share <= 0.60, "primary column occupies 55 to 60 percent");
        for (auto *card : recommendationCards) {
            const QRect rect(card->mapTo(resultScroll->viewport(), QPoint()), card->size());
            check(resultScroll->viewport()->rect().contains(rect), "all cards visible on first screen at 1600x900");
            check(card->height() <= 420, "cards must not stretch vertically");
            check(card->findChildren<QPushButton *>("viewRecommendedRecipe").size() == 1, "one recipe entry per card");
        }
        auto *quickCard = recommendation->findChild<QFrame *>("recommendationQuickCard");
        check(recommendation->rect().contains(QRect(quickCard->mapTo(recommendation, QPoint()), quickCard->size())),
              "quick actions always visible");
        check(quickCard->findChildren<QPushButton *>("quickAdjustment").size() == 3 &&
              quickCard->findChild<QPushButton *>("swapRecommendations"), "four quick actions");
        check(!primaryCard->findChild<QLabel *>("recommendationBrief")->text().isEmpty(), "reason visible without expansion");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            check(window.grab().save(output + "/recommendation-results-1600.png"), "capture results at 1600x900");
        }
        const auto adjustments = quickCard->findChildren<QPushButton *>("quickAdjustment");
        adjustments[1]->click();
        adjustments[2]->click();
        for (int i = 0; i < 5; ++i) app.processEvents();
        check(adjustments[1]->isChecked() && adjustments[2]->isChecked(), "time and special both show active state");
        check(recommendation->findChild<QLabel *>("recommendationConditions")->text().contains(QStringLiteral("20 分钟")),
              "effective shorter time displayed in summary");
        if (!output.isEmpty())
            check(window.grab().save(output + "/recommendation-active-adjustments.png"), "capture active quick adjustments");
        recommendation->findChild<QPushButton *>("resetRecommendationAdjustments")->click();
        check(!adjustments[1]->isChecked() && !adjustments[2]->isChecked(), "undo clears active states");
        for (const auto page :
             {MainWindow::Page::Home, MainWindow::Page::Recommendations, MainWindow::Page::Recipes, MainWindow::Page::CheckIn, MainWindow::Page::Statistics,
              MainWindow::Page::Profile, MainWindow::Page::Inventory}) {
            window.showPage(page);
            for (QSize size : {QSize(980, 620), QSize(1280, 800), QSize(1600, 900)}) {
                window.resize(size);
                for (int i = 0; i < 5; ++i)
                    app.processEvents();
                check(window.size() == size, "all pages fit target window");
                for (auto *area : window.findChildren<QScrollArea *>())
                    if (area->isVisible())
                        check(area->horizontalScrollBar()->maximum() == 0,
                              "page must not scroll horizontally");
                if (page == MainWindow::Page::Recommendations) {
                    check(recommendation->rect().contains(QRect(quickCard->mapTo(recommendation, QPoint()), quickCard->size())),
                          "quick actions fit every supported window size");
                    for (auto *button : quickCard->findChildren<QPushButton *>("quickAdjustment"))
                        check(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 24,
                              "quick action text must not be clipped");
                    const auto cards = recommendation->findChild<QWidget *>("recommendationBoard")
                        ->findChildren<QFrame *>("recommendationCard");
                    for (int index = 0; index < cards.size(); ++index) {
                        auto *toggle = cards[index]->findChild<QPushButton *>("recommendationExplanation");
                        auto *panel = cards[index]->findChild<QFrame *>("recommendationReasonPanel");
                        check(toggle && panel && panel->isHidden(), "every card offers collapsed recommendation evidence");
                        toggle->click();
                        for (int i = 0; i < 5; ++i) app.processEvents();
                        check(!panel->isHidden() && resultScroll->horizontalScrollBar()->maximum() == 0,
                              "expanded evidence wraps within every window size");
                        check(recommendation->rect().contains(QRect(quickCard->mapTo(recommendation, QPoint()), quickCard->size())),
                              "expanded evidence keeps quick actions visible");
                        if (!output.isEmpty() && size.width() == 1600)
                            check(cards[index]->grab().save(output + QString("/recommendation-evidence-%1.png").arg(index)),
                                  "capture each card's recommendation evidence");
                        toggle->click();
                        for (int i = 0; i < 5; ++i) app.processEvents();
                        check(panel->isHidden(), "evidence can be collapsed again");
                    }
                    resultScroll->verticalScrollBar()->setValue(0);
                }
                if (!output.isEmpty())
                    check(window.grab().save(output +
                                             QString("/page-%1-%2.png").arg(int(page)).arg(size.width())),
                          "save page screenshot");
            }
        }
        window.showPage(MainWindow::Page::Recommendations);
        recommendation->findChild<QPushButton *>("editRecommendationConditions")->click();
        window.resize(980, 620);
        app.processEvents();
        if (!output.isEmpty())
            check(window.grab().save(output + "/recommendation-setup.png"), "save setup screenshot");
        recommendation->findChild<QPushButton *>("generateRecommendations")->click();
        QPushButton *recipeDetail = nullptr;
        for (auto *button : recommendation->findChildren<QPushButton *>())
            if (button->objectName() == QStringLiteral("viewRecommendedRecipe")) { recipeDetail = button; break; }
        check(recipeDetail, "recommendation detail action available");
        auto *explanation = recommendation->findChild<QPushButton *>("recommendationExplanation");
        explanation->click();
        for (int i = 0; i < 5; ++i) app.processEvents();
        resultScroll->verticalScrollBar()->setValue(resultScroll->verticalScrollBar()->maximum());
        const int resultPosition = resultScroll->verticalScrollBar()->value();
        check(resultPosition > 0, "small result page can scroll expanded explanation");
        recipeDetail->click();
        for (int i = 0; i < 5; ++i) app.processEvents();
        auto *detailScroll = recommendation->recipeDetails()->findChild<QScrollArea *>("recipeDetailsScroll");
        check(detailScroll->verticalScrollBar()->value() == 0, "details begin at top");
        const auto ingredientLabels = recommendation->recipeDetails()->findChildren<QLabel *>("recipeSection");
        check(ingredientLabels.size() == 3, "recipe sections present");
        const int nutritionY = ingredientLabels[0]->mapTo(detailScroll->widget(), QPoint()).y();
        const int ingredientsY = ingredientLabels[1]->mapTo(detailScroll->widget(), QPoint()).y();
        const int stepsY = ingredientLabels[2]->mapTo(detailScroll->widget(), QPoint()).y();
        check(ingredientsY < stepsY && stepsY < nutritionY, "ingredients and steps precede nutrition");
        check(recommendation->findChild<QStackedWidget *>("recommendationStages")->currentIndex() == 2,
              "recommendation recipe is shown inline");
        if (!output.isEmpty())
            check(window.grab().save(output + "/recommendation-detail.png"), "save detail screenshot");
        detailScroll->verticalScrollBar()->setValue(detailScroll->verticalScrollBar()->maximum());
        check(detailScroll->verticalScrollBar()->value() > 0, "recipe details can scroll");
        recommendation->findChild<QPushButton *>("backToRecommendations")->click();
        for (int i = 0; i < 5; ++i) app.processEvents();
        check(resultScroll->verticalScrollBar()->value() == resultPosition, "return restores result scroll position");
        recipeDetail->click();
        for (int i = 0; i < 5; ++i) app.processEvents();
        check(detailScroll->verticalScrollBar()->value() == 0, "reopening same recipe resets scroll");
        recommendation->findChild<QPushButton *>("backToRecommendations")->click();
        FoodCandidate apple; apple.name = QStringLiteral("苹果"); apple.unit = QStringLiteral("个"); apple.quantity = 2;
        apple.category = QStringLiteral("水果");
        window.addFood(apple);
        window.showPage(MainWindow::Page::Inventory);
        auto *filter = window.findChild<QComboBox *>("inventoryCategoryFilter");
        auto *table = window.findChild<QTableWidget *>("inventoryTable");
        filter->setCurrentText(QStringLiteral("水果"));
        check(table->rowCount() == 1 && table->item(0, 0)->text() == QStringLiteral("苹果"), "category filter");
        const QRect cellRect = table->visualItemRect(table->item(0, 0));
        const QPoint hoverPoint(cellRect.center().x(), cellRect.center().y());
        QMouseEvent hover(QEvent::MouseMove, hoverPoint, table->viewport()->mapToGlobal(hoverPoint),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &hover);
        app.processEvents();
        const QColor hoverColor = table->viewport()->grab().toImage().pixelColor(hoverPoint);
        check(hoverColor.lightness() > 180, "inventory hover stays light");
        table->selectRow(0);
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            dialog->findChild<QComboBox *>("inventoryEditCategory")->setCurrentText(QStringLiteral("其他"));
            dialog->findChild<QDoubleSpinBox *>("inventoryEditQuantity")->setValue(4);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        window.findChild<QPushButton *>("inventoryEdit")->click();
        check(table->rowCount() == 0, "edited item leaves old category");
        filter->setCurrentText(QStringLiteral("其他"));
        check(table->rowCount() == 1 && table->item(0, 2)->text().startsWith("4"), "edited item shown in new category");
        FoodCandidate tomato; tomato.name = QStringLiteral("番茄"); tomato.unit = QStringLiteral("盒");
        tomato.category = QStringLiteral("其他"); tomato.quantity = 12.333333333333334; tomato.gramsPerUnit = 150;
        window.addFood(tomato);
        int tomatoRow = -1;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0)->text() == QStringLiteral("番茄")) tomatoRow = row;
        check(tomatoRow >= 0 && table->item(tomatoRow, 2)->text() == QStringLiteral("约 12 盒当量 + 50 克"),
              "persisted fractional inventory displayed correctly");
        table->selectRow(tomatoRow);
        app.processEvents();
        if (!output.isEmpty()) window.grab().save(output + "/inventory-filled.png");
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                app.processEvents();
                if (!output.isEmpty()) { QDir().mkpath(output); dialog->grab().save(output + "/inventory-edit-dialog.png"); }
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            }
        });
        window.findChild<QPushButton *>("inventoryEdit")->click();
        QSqlQuery saved(QSqlDatabase::database("ShixuInventoryConnection"));
        saved.prepare("SELECT quantity,totalWeightGrams FROM foods WHERE name=?");
        saved.addBindValue(QStringLiteral("番茄"));
        check(saved.exec() && saved.next() && saved.value(0).toDouble() == tomato.quantity &&
                  saved.value(1).toDouble() == tomato.quantity * tomato.gramsPerUnit,
              "saving unchanged item preserves quantity and calculated weight");
        window.showPage(MainWindow::Page::Profile);
        auto *nicknameLabel = window.findChild<QLabel *>("profileNickname");
        auto *avatarLabel = window.findChild<QLabel *>("profileAvatar");
        check(nicknameLabel && avatarLabel && !window.findChild<QLabel *>("profileGrowthStatus"),
              "profile identity displayed without duplicate growth summary");
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                dialog->setTextValue(QStringLiteral("测试厨师"));
                app.processEvents();
                if (!output.isEmpty()) dialog->grab().save(output + "/nickname-dialog.png");
                dialog->accept();
            }
        });
        QMouseEvent editName(QEvent::MouseButtonDblClick, QPointF(4, 4), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(nicknameLabel, &editName);
        check(nicknameLabel->text() == QStringLiteral("测试厨师"), "double-click changes nickname");
        const QString avatarPath = data.filePath("avatar-source.png");
        QImage picture(80, 40, QImage::Format_RGB32); picture.fill(Qt::red);
        check(picture.save(avatarPath), "create avatar fixture");
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if (dialog) { dialog->selectFile(avatarPath); static_cast<QDialog *>(dialog)->accept(); }
        });
        QMouseEvent editAvatar(QEvent::MouseButtonDblClick, QPointF(4, 4), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(avatarLabel, &editAvatar);
        UserProfileStore stored(data.filePath("profile.json"));
        check(stored.error.isEmpty() && stored.value.nickname == QStringLiteral("测试厨师") &&
                  !stored.value.avatarImage.isEmpty(), "imported avatar and nickname survive reload");
        auto *excludedGrid = window.findChild<QWidget *>("profileExcluded");
        auto *profileScroll = window.findChild<QScrollArea *>("profileScroll");
        check(excludedGrid && profileScroll && !excludedGrid->findChild<QScrollArea *>(),
              "profile uses one page scroll and no nested ingredient scroll");
        auto options = excludedGrid->findChildren<QCheckBox *>("profileExcludedOption");
        QCheckBox *tomatoOption = nullptr;
        for (auto *option : options)
            if (option->text() == QStringLiteral("番茄")) tomatoOption = option;
        check(tomatoOption && options.size() > 2, "ingredient options exist");
        tomatoOption->setChecked(true);
        for (const QSize size : {QSize(980, 620), QSize(1280, 800)}) {
            window.resize(size);
            for (int i = 0; i < 3; ++i) app.processEvents();
            check(tomatoOption->isChecked() && options[0]->geometry().top() == options[1]->geometry().top(),
                  "multiple columns reflow without losing checked options");
            check(profileScroll->horizontalScrollBar()->maximum() == 0, "profile fits width");
        }
        window.findChild<QPushButton *>("saveProfile")->click();
        check(QFileInfo::exists(data.filePath("profile.json")), "profile saved through UI");
        UserProfileStore afterPreferences(data.filePath("profile.json"));
        check(afterPreferences.value.avatarImage == stored.value.avatarImage &&
                  afterPreferences.value.nickname == stored.value.nickname &&
                  afterPreferences.value.excluded.contains(QStringLiteral("番茄")), "preference save keeps identity and selected ingredient");
        window.showPage(MainWindow::Page::Statistics);
        window.resize(1280, 800);
        auto *growthScroll = window.findChild<QStackedWidget *>("pageStack")->currentWidget()->findChild<QScrollArea *>();
        auto *goalButton = window.findChild<QPushButton *>("editNutritionGoals");
        if (goalButton) {
            growthScroll->ensureWidgetVisible(goalButton);
            app.processEvents();
            if (!output.isEmpty()) window.grab().save(output + "/growth-nutrition.png");
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (dialog) {
                    app.processEvents();
                    if (!output.isEmpty()) dialog->grab().save(output + "/nutrition-goals-dialog.png");
                    dialog->reject();
                }
            });
            goalButton->click();
        }
        std::cout << "Packaging layout checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    QDir::setCurrent(original);
    return result;
}
