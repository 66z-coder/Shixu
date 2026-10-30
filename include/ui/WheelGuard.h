#pragma once
#include <QApplication>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QScrollArea>
#include <QWheelEvent>
namespace Shixu {
// Browsing a form must never silently change its values, even for focused fields.
class WheelGuard final : public QObject {
public:
    explicit WheelGuard(QObject* parent):QObject(parent){}
protected:
    bool eventFilter(QObject* object,QEvent* event) override {
        if(event->type()!=QEvent::Wheel)return false;
        auto* widget=qobject_cast<QWidget*>(object);
        if(!widget || widget->window()->windowType()==Qt::Popup)return false;
        QWidget* field=widget;
        while(field && !qobject_cast<QComboBox*>(field) && !qobject_cast<QAbstractSpinBox*>(field))field=field->parentWidget();
        if(!field)return false;
        auto* wheel=static_cast<QWheelEvent*>(event);
        for(auto* parent=field->parentWidget();parent;parent=parent->parentWidget()) {
            if(auto* area=qobject_cast<QScrollArea*>(parent)) {
                auto* viewport=area->viewport();
                QWheelEvent forwarded(viewport->mapFromGlobal(wheel->globalPosition().toPoint()),wheel->globalPosition(),wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted(),wheel->source());
                QApplication::sendEvent(viewport,&forwarded);
                break;
            }
        }
        wheel->accept();return true;
    }
};
inline void installWheelGuard() {
    static const auto* guard=[] {auto* value=new WheelGuard(qApp);qApp->installEventFilter(value);return value;}();
    Q_UNUSED(guard);
}
}
