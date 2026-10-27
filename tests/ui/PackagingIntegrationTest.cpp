#include "ui/MainWindow.h"
#include "ui/InventoryWidget.h"
#include "recognition/PackagingEntryWidget.h"
#include "inventory/inventory.h"
#include "recommendation/RecommendationContextBuilder.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QTableWidget>
#include <QStackedWidget>
#include <QLabel>
#include <limits>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void sql(QSqlQuery& query, const QString& text) {
    if (!query.exec(text)) throw std::runtime_error(query.lastError().text().toStdString());
}
FoodCandidate food(QString name, double quantity, QString unit, double per = 0) {
    FoodCandidate f; f.name=name; f.quantity=quantity; f.unit=unit; f.gramsPerUnit=per;
    return f;
}
void queue(PackagingEntryWidget* w, const QString& name, double qty, const QString& unit, double weight) {
    w->findChild<QLineEdit*>("foodName")->setText(name);
    w->findChild<QComboBox*>("foodUnit")->setCurrentText(unit);
    w->findChild<QDoubleSpinBox*>("foodQuantity")->setValue(qty);
    w->findChild<QDoubleSpinBox*>("gramsPerUnit")->setValue(weight);
    w->findChild<QComboBox*>("dateMode")->setCurrentIndex(3);
    w->findChild<QCheckBox*>("confirmed")->setChecked(true);
    auto* save=w->findChild<QPushButton*>("saveFood");
    check(save->isEnabled(),"food must be ready to queue"); save->click();
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QStandardPaths::setTestModeEnabled(true);
    const QString original=QDir::currentPath();
    QTemporaryDir temporary;
    try {
        check(temporary.isValid() && QDir::setCurrent(temporary.path()),"isolated directory");
        // Create the pre-weight schema before initializing B.
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE","legacy");
            db.setDatabaseName("shixu.db"); check(db.open(),"legacy database open");
            QSqlQuery q(db);
            sql(q,"CREATE TABLE foods(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,quantity REAL NOT NULL,unit TEXT NOT NULL,expiryDate TEXT)");
            sql(q,"INSERT INTO foods(name,quantity,unit) VALUES('legacy',2,'个')");
        }
        QSqlDatabase::removeDatabase("legacy");
        {
            InventoryManager inventory;
            check(inventory.getAllItems().size()==1 && inventory.getAllItems()[0].totalWeightGrams==0,
                  "migration preserves old record and unknown weight");
            QString error;
            auto tomato=food("番茄",10,"个",150); tomato.totalWeightGrams=999;
            tomato.expiryDate=QDate::currentDate();
            auto expired=food("鸡蛋",2,"个",50); expired.expiryDate=QDate::currentDate().addDays(-1);
            check(inventory.addFoods({tomato,expired,food("米",2.5,"kg"),food("面粉",250,"g"),
                                      food("土豆",2,"斤"),food("未知",2,"袋"),food("牛奶",200,"ml")},&error),
                  "batch persisted");
            const auto items=inventory.getAllItems();
            check(items[1].totalWeightGrams==1500 && items[1].gramsPerUnit==150 && items[1].unit=="个",
                  "derive weight rather than trust stale total");
            check(items[3].totalWeightGrams==2500 && items[4].totalWeightGrams==250
                  && items[5].totalWeightGrams==1000 && items[6].totalWeightGrams==0
                  && items[7].totalWeightGrams==0,"mass conversion and unknown units");
            check(inventory.getExpiringItems(QDate::currentDate(),3)[0].totalWeightGrams==1500
                  && inventory.getExpiredItems(QDate::currentDate())[0].gramsPerUnit==50,
                  "all queries return weights");
            auto invalid=food("invalid",1,"g");
            invalid.gramsPerUnit=std::numeric_limits<double>::quiet_NaN();
            check(!inventory.addFoods({food("valid",1,"g"),invalid},&error)
                  && !error.isEmpty() && inventory.getAllItems().size()==8,"invalid batch not partially saved");
            QSqlQuery q(QSqlDatabase::database("ShixuInventoryConnection"));
            sql(q,"CREATE TRIGGER reject_insert BEFORE INSERT ON foods WHEN NEW.name='fail' BEGIN SELECT RAISE(ABORT,'test failure'); END");
            check(!inventory.addFoods({food("rolled back",1,"g"),food("fail",1,"g")},&error)
                  && inventory.getAllItems().size()==8,"SQL failure rolls back previous inserts");
            sql(q,"DROP TRIGGER reject_insert");
        }
        {
            InventoryManager inventory;
            check(inventory.getAllItems().size()==8 && inventory.getAllItems()[1].totalWeightGrams==1500,
                  "restart retains weights; migration repeat is safe");
            auto built=Shixu::RecommendationEngine::readUserContext(inventory);
            bool found=false;
            for(const auto& item:built.context.inventory)
                if(item.foodName=="番茄") found=item.amount==1500;
            check(found && built.expiredBatches==1,"D consumes saved mass and excludes expired");
            auto legacyJin=Shixu::RecommendationEngine::buildUserContext({food("土豆",2,"斤")});
            check(legacyJin.context.inventory[0].amount==1000,"legacy jin fallback");
        }
        // Submission must remain pending until an explicit acknowledgement.
        {
            PackagingEntryWidget w;
            queue(&w,"苹果",1,"个",100);
            int requests=0;
            QObject::connect(&w,&PackagingEntryWidget::foodsConfirmed,[&](const auto&){++requests;});
            auto* submit=w.findChild<QPushButton*>("submitCandidate");
            auto* list=w.findChild<QListWidget*>("pendingFoods");
            submit->click();submit->click();
            check(requests==1 && list->count()==1 && !w.isEnabled(),"pending blocks repeat and keeps data");
            w.completeSubmission(false,"test error");
            check(w.isEnabled() && list->count()==1 && submit->isEnabled(),"failed save can retry");
            submit->click();w.completeSubmission(true);submit->click();
            check(requests==2 && list->count()==0,"successful acknowledgement clears exactly once");
        }
        {
            MainWindow window;
            auto* entry=window.findChild<PackagingEntryWidget*>();
            check(entry,"real entry page installed");
            window.showPage(MainWindow::Page::FoodEntry);
            entry->findChild<QLineEdit*>("foodName")->setText("西红柿");
            window.showPage(MainWindow::Page::Inventory);
            window.showPage(MainWindow::Page::FoodEntry);
            check(entry->findChild<QLineEdit*>("foodName")->text()=="西红柿","page switch preserves draft");
            entry->findChild<QPushButton*>("resetForm")->click();
            queue(entry,"西红柿",10,"个",150);
            queue(entry,"鸡蛋",2,"个",50);
            QSqlQuery q(QSqlDatabase::database("ShixuInventoryConnection"));
            sql(q,"CREATE TRIGGER ui_failure BEFORE INSERT ON foods WHEN NEW.name='鸡蛋' BEGIN SELECT RAISE(ABORT,'test UI failure'); END");
            auto* submit=entry->findChild<QPushButton*>("submitCandidate");
            auto* list=entry->findChild<QListWidget*>("pendingFoods");
            submit->click();
            sql(q,"SELECT COUNT(*) FROM foods");q.next();check(q.value(0).toInt()==8,"UI failure leaves no partial rows");q.finish();
            check(list->count()==2 && submit->isEnabled(),"UI failed batch retained");
            sql(q,"DROP TRIGGER ui_failure");
            submit->click();
            check(list->count()==0 && !submit->isEnabled(),"UI retry clears saved batch");
            check(window.findChild<QStackedWidget*>("pageStack")->currentWidget()==entry,"stay on entry page");
            check(entry->findChild<QLabel*>("entryStatus")->text().contains("2"),"success count shown");
            auto* table=window.findChild<InventoryWidget*>()->findChild<QTableWidget*>();
            check(table->rowCount()==10,"inventory refreshed without navigating");
            sql(q,"SELECT name,quantity,unit,gramsPerUnit,totalWeightGrams FROM foods ORDER BY id DESC LIMIT 2");
            bool found=false;
            while(q.next()) if(q.value(0).toString()=="番茄")
                found=q.value(1).toDouble()==10 && q.value(2).toString()=="个"
                      && q.value(3).toDouble()==150 && q.value(4).toDouble()==1500;
            check(found,"UI normalized name and complete weight persisted");
        }
        {
            InventoryManager inventory;
            check(inventory.getAllItems().size()==10,"UI records persist on reopen without duplicate retry");
        }
        QDir::setCurrent(original);
        std::cout<<"PASS: migration, atomic save, weight roundtrip, pending/retry, real page integration"<<std::endl;
        return 0;
    } catch(const std::exception& e) {
        QDir::setCurrent(original);std::cerr<<e.what()<<std::endl;return 1;
    }
}
