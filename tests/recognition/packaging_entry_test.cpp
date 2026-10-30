#include "recognition/PackagingEntryWidget.h"
#include <QApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QKeyEvent>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
 QApplication app(argc,argv);
 try{
 PackagingEntryWidget w;
 auto* name=w.findChild<QLineEdit*>("foodName");
 auto* unit=w.findChild<QComboBox*>("foodUnit");
 auto* mode=w.findChild<QComboBox*>("dateMode");
 auto* expiry=w.findChild<QLineEdit*>("expiryDate");
 auto* production=w.findChild<QLineEdit*>("productionDate");
 auto* category=w.findChild<QComboBox*>("foodCategory");
 auto* raw=w.findChild<QPlainTextEdit*>("packageText");
 auto* confirmed=w.findChild<QCheckBox*>("confirmed");
 auto* save=w.findChild<QPushButton*>("saveFood");
 auto* submit=w.findChild<QPushButton*>("submitCandidate");
 auto* reset=w.findChild<QPushButton*>("resetForm");
 auto* edit=w.findChild<QPushButton*>("editFood");
 auto* remove=w.findChild<QPushButton*>("deleteFood");
 auto* list=w.findChild<QListWidget*>("pendingFoods");
 auto* quantity=w.findChild<QDoubleSpinBox*>("foodQuantity");
 auto parse=[&]{for(auto* b:w.findChildren<QPushButton*>())if(b->text()=="解析日期信息")b->click();};
 check(!w.findChild<QComboBox*>("countingConvention"),"counting option removed");
 int submissions=0;std::vector<FoodCandidate> received;
 QObject::connect(&w,&PackagingEntryWidget::foodsConfirmed,[&](const std::vector<FoodCandidate>& f){++submissions;received=f;w.completeSubmission(true);});
 name->setText("西红柿");unit->setCurrentText("公斤");
 check(category->currentText()==QStringLiteral("蔬菜"),"automatic category suggestion");
 category->setCurrentText(QStringLiteral("其他"));
 raw->setPlainText("有效期至：2027-01-01");parse();
 check(!save->isEnabled(),"confirmation required");
 confirmed->setChecked(true);save->click();
 check(list->count()==1 && submissions==0 && name->text().isEmpty() && submit->isEnabled(),"add queues without emit and clears form");
 name->setText("牛奶");unit->setCurrentText("盒");
 raw->setPlainText("生产日期 2026-10-20");parse();confirmed->setChecked(true);
 check(!save->isEnabled() && !submit->isEnabled(),"incomplete draft blocks save and batch submit");
 raw->setPlainText("生产日期 2026-10-20 保质期 30天");parse();confirmed->setChecked(true);
 check(save->isEnabled(),"calculation no longer needs counting choice");
 save->click();check(list->count()==2,"second item preserved alongside first");
 name->setText("苹果");unit->setCurrentText("个");mode->setCurrentIndex(3);confirmed->setChecked(true);save->click();
 check(list->count()==3,"manual unknown expiry allowed explicitly");
 list->setCurrentRow(0);edit->click();check(name->text()=="番茄" && !submit->isEnabled(),"edit loads normalized data and blocks batch");
 quantity->setValue(2.5);confirmed->setChecked(true);save->click();
 check(list->count()==3 && list->item(0)->text().contains("2.5"),"save replaces rather than appends");
 list->setCurrentRow(1);edit->click();check(mode->currentIndex()==2,"computed date basis restored");
 name->setText("unsaved");reset->click();check(list->count()==3,"cancel edit retains saved item");
 list->setCurrentRow(2);remove->click();check(list->count()==2,"delete chosen item");
 submit->click();
 check(submissions==1 && received.size()==2 && received[0].name=="番茄" && received[0].quantity==2.5
 && received[0].category==QStringLiteral("其他")
 && received[0].unit=="kg" && received[1].expiryDate==QDate(2026,11,19),"one batch standardized output");
 check(list->count()==0 && !submit->isEnabled(),"batch clears and cannot resubmit");
 raw->setPlainText("有效期至 2027-01-01 有效期至 2027-02-01");parse();
 check(mode->currentIndex()==0 && expiry->text().isEmpty(),"conflicting dates still require supplement");
 reset->click();name->setText("苹果");unit->setCurrentText("一些");mode->setCurrentIndex(3);confirmed->setChecked(true);
 check(!save->isEnabled(),"unknown unit blocked");
 reset->click();
 name->setText("青菜"); unit->setCurrentText("个"); mode->setCurrentIndex(1);
 check(category->currentText()==QStringLiteral("蔬菜") && expiry->isReadOnly() && production->parentWidget()->isHidden(),"known expiry only and category");
 confirmed->setChecked(true); check(!save->isEnabled(),"date requires explicit choice");
 expiry->setText("2027-01-01"); confirmed->setChecked(true); check(save->isEnabled(),"explicit expiry needs no production");
 reset->click();
 check(quantity->decimals()==1 && quantity->minimum()==0.1,"one decimal quantity");
 name->setText("西红柿");unit->setCurrentText("个");quantity->setValue(10);mode->setCurrentIndex(4);
 confirmed->setChecked(true);check(!save->isEnabled(),"missing storage days blocked");
 auto* days=w.findChild<QSpinBox*>("storageDays");days->setValue(3);
 check(!confirmed->isChecked(),"changed days require confirmation");
 const QDate expected=QDate::currentDate().addDays(3);
 confirmed->setChecked(true);save->click();
 check(list->count()==1 && list->item(0)->text().contains(expected.toString(Qt::ISODate)),"today plus three days");
 list->setCurrentRow(0);edit->click();
 check(mode->currentIndex()==4 && days->value()==3,"restore storage days on edit");
 confirmed->setChecked(true);save->click();submit->click();
 check(received.size()==1 && received[0].quantity==10 && received[0].expiryDate==expected,"manual storage candidate output");
 reset->click();
 auto* weight=w.findChild<QDoubleSpinBox*>("gramsPerUnit");
 auto* total=w.findChild<QLabel*>("totalWeight");
 name->setText("西红柿");unit->setCurrentText("个");quantity->setValue(10);mode->setCurrentIndex(3);
 check(!weight->isHidden() && total->text().contains("未知"),"count unit offers optional weight");
 weight->setValue(150);check(total->text()=="1500.0 g","ten items at 150g");
 confirmed->setChecked(true);quantity->setValue(2);
 check(!confirmed->isChecked() && total->text()=="300.0 g","quantity recalculates and resets confirmation");
 quantity->setValue(10);confirmed->setChecked(true);save->click();
 check(weight->value()==0 && list->item(0)->text().contains("1500.0 g"),"clear form retains queued weight");
 list->setCurrentRow(0);edit->click();
 check(weight->value()==150 && total->text()=="1500.0 g","edit restores weight");
 weight->setValue(120);confirmed->setChecked(true);save->click();submit->click();
 check(received[0].quantity==10 && received[0].unit=="个" && received[0].gramsPerUnit==120
       && received[0].totalWeightGrams==1200,"batch carries original units and weights");
 name->setText("苹果");unit->setCurrentText("盒");weight->setValue(500);
 unit->setCurrentText("袋");check(weight->value()==0,"changing package clears weight");
 weight->setValue(600);name->setText("大米");check(weight->value()==0,"changing food clears weight");
 weight->setValue(500);unit->setCurrentText("公斤");quantity->setValue(2.5);mode->setCurrentIndex(3);
 check(weight->isHidden() && total->text()=="2500.0 g","kg aliases convert without extra field");
 confirmed->setChecked(true);save->click();submit->click();
 check(received[0].totalWeightGrams==2500 && received[0].gramsPerUnit==0,"direct mass output");
 name->setText("面粉");unit->setCurrentText("克");quantity->setValue(250);
 check(total->text()=="250.0 g","g conversion");
 unit->setCurrentText("斤");quantity->setValue(2);
 check(total->text()=="1000.0 g","jin conversion");
 unit->setCurrentText("ml");check(weight->isHidden() && total->text().contains("未知"),"no assumed liquid density");
 mode->setCurrentIndex(3);confirmed->setChecked(true);save->click();submit->click();
 check(received[0].totalWeightGrams==0,"unknown mass remains unknown");
 name->setText("鸡蛋");unit->setCurrentText("个");mode->setCurrentIndex(3);
 confirmed->setChecked(true);save->click();submit->click();
 check(received[0].gramsPerUnit==0 && received[0].totalWeightGrams==0,"missing count weight remains unknown");
 reset->click();
 name->setText("西红柿");unit->setCurrentText("个");mode->setCurrentIndex(4);
 auto key=[](QWidget* target,int code,const QString& text=QString()) {
     QKeyEvent press(QEvent::KeyPress,code,Qt::NoModifier,text);
     QApplication::sendEvent(target,&press);
     QKeyEvent release(QEvent::KeyRelease,code,Qt::NoModifier,text);
     QApplication::sendEvent(target,&release);
 };
 auto* dayEditor=days->findChild<QLineEdit*>();
 check(dayEditor->text().isEmpty() && !dayEditor->placeholderText().isEmpty(),"days use real placeholder");
 key(dayEditor,Qt::Key_3,"3");
 check(days->value()==3,"type directly into empty days");
 confirmed->setChecked(true);
 key(dayEditor,Qt::Key_Backspace);
 check(dayEditor->text().isEmpty() && days->value()==0 && !save->isEnabled() && !confirmed->isChecked(),
       "backspace clears days and invalidates previous date");
 key(dayEditor,Qt::Key_5,"5");
 check(days->value()==5,"retype days after clearing");
 auto* period=w.findChild<QSpinBox*>("shelfLife");
 mode->setCurrentIndex(2);
 auto* periodEditor=period->findChild<QLineEdit*>();
 key(periodEditor,Qt::Key_7,"7");check(period->value()==7,"type shelf life");
 periodEditor->selectAll();key(periodEditor,Qt::Key_Delete);
 check(period->value()==0 && periodEditor->text().isEmpty(),"delete shelf life");
 auto* weightEditor=weight->findChild<QLineEdit*>();
 weight->setValue(150);weightEditor->selectAll();key(weightEditor,Qt::Key_Delete);
 check(weight->value()==0 && weightEditor->text().isEmpty() && total->text().contains("未知"),
       "delete weight removes prior total");
 key(weightEditor,Qt::Key_2,"2");check(weight->value()==2,"retype optional weight");
 std::cout<<"PASS: batch add/edit/delete/cancel/submit, date calculation, incomplete input and unknown expiry\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

