#include "ui/WheelGuard.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QLineEdit>
#include <QKeyEvent>
#include <iostream>
int main(int argc,char** argv){QApplication app(argc,argv);Shixu::installWheelGuard();Shixu::installWheelGuard();
 QScrollArea area;area.resize(400,200);auto* body=new QWidget;auto* layout=new QVBoxLayout(body);
 auto* combo=new QComboBox;combo->addItems({"A","B","C"});combo->setCurrentIndex(1);combo->setEditable(true);
 auto* number=new QSpinBox;number->setValue(5);auto* decimal=new QDoubleSpinBox;decimal->setValue(2.5);
 layout->addWidget(combo);layout->addWidget(number);layout->addWidget(decimal);layout->addSpacing(800);area.setWidget(body);area.setWidgetResizable(true);area.show();app.processEvents();
 for(auto* field:QList<QWidget*>{combo,combo->lineEdit(),number,decimal})for(bool focus:{false,true}) {
   if(focus)field->setFocus();else field->clearFocus();
   area.verticalScrollBar()->setValue(0);
   QWheelEvent wheel(QPointF(5,5),field->mapToGlobal(QPoint(5,5)),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
   QApplication::sendEvent(field,&wheel);
   if(combo->currentIndex()!=1||number->value()!=5||decimal->value()!=2.5||area.verticalScrollBar()->value()==0){std::cerr<<"wheel changed field or blocked page scroll";return 1;}
 }
 QKeyEvent key(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QApplication::sendEvent(combo,&key);
 if(combo->currentIndex()!=2)return 2;
 number->stepUp();if(number->value()!=6)return 3;
 std::cout<<"Wheel blocked for focused/unfocused fields; page scroll and explicit edits preserved\n";return 0;}
