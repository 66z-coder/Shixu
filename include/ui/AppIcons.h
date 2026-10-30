#pragma once

#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QString>
#include <initializer_list>

namespace Shixu::Ui {
enum class Icon { Home, Scan, Fridge, Recipes, Spark, Growth, Profile, Meat, Vegetable, Soup, Rice, Fish, Plate };

// Paint in logical coordinates so the same small icons remain sharp at every display scale.
class LineIconEngine final : public QIconEngine {
public:
    explicit LineIconEngine(Icon kind) : m_kind(kind) {}
    QIconEngine *clone() const override { return new LineIconEngine(m_kind); }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const qreal side = qMin(rect.width(), rect.height());
        p->translate(rect.center().x() - side / 2, rect.center().y() - side / 2);
        p->scale(side / 24, side / 24);
        const QColor color(mode == QIcon::Disabled ? "#95A9B4" : state == QIcon::On ? "#185D88" : "#527D94");
        p->setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        auto line = [p](qreal x1, qreal y1, qreal x2, qreal y2) { p->drawLine(QPointF(x1,y1), QPointF(x2,y2)); };
        auto path = [p](std::initializer_list<QPointF> points) {
            QPainterPath shape; bool first = true;
            for (const auto &point : points) { if (first) shape.moveTo(point); else shape.lineTo(point); first = false; }
            p->drawPath(shape);
        };
        switch (m_kind) {
        case Icon::Home:
            path({{3,10},{12,3},{21,10}}); path({{5,9},{5,21},{10,21},{10,14},{14,14},{14,21},{19,21},{19,9}}); break;
        case Icon::Scan:
            path({{3,8},{3,3},{8,3}}); path({{16,3},{21,3},{21,8}});
            path({{3,16},{3,21},{8,21}}); path({{16,21},{21,21},{21,16}});
            line(5,12,19,12); line(8,7,16,7); line(8,17,16,17); break;
        case Icon::Fridge:
            p->drawRoundedRect(QRectF(5,2,14,20),2,2); line(5,12,19,12); line(8,6,8,9); line(8,15,8,18); break;
        case Icon::Recipes:
            p->drawRoundedRect(QRectF(5,3,15,18),2,2); line(3,7,7,7); line(3,12,7,12); line(3,17,7,17);
            line(11,8,16,8); line(11,12,16,12); line(11,16,14,16); break;
        case Icon::Spark:
            path({{12,3},{14.5,9.5},{21,12},{14.5,14.5},{12,21},{9.5,14.5},{3,12},{9.5,9.5},{12,3}});
            line(20,2,20,6); line(18,4,22,4); break;
        case Icon::Growth:
            path({{4,4},{4,21},{21,21}}); path({{7,16},{12,11},{15,14},{21,6}}); path({{16,6},{21,6},{21,11}}); break;
        case Icon::Profile:
            p->drawEllipse(QRectF(8,3,8,8)); p->drawArc(QRectF(4,13,16,14),0,180*16); line(4,20,20,20); break;
        case Icon::Vegetable: {
            QPainterPath leaf; leaf.moveTo(5,18); leaf.cubicTo(0,6,15,2,21,3); leaf.cubicTo(21,14,16,22,5,18);
            p->drawPath(leaf); line(3,21,16,8); line(9,15,9,10); break;
        }
        case Icon::Meat:
            p->drawEllipse(QRectF(9,3,12,13)); path({{10,13},{4,19},{6,21},{12,15}}); p->drawEllipse(QRectF(14,7,3,4)); break;
        case Icon::Soup:
        case Icon::Rice: {
            if (m_kind == Icon::Rice) p->drawArc(QRectF(5,5,14,12),0,180*16);
            else { line(8,3,7,7); line(12,2,11,6); line(16,3,15,7); }
            QPainterPath bowl; bowl.moveTo(3,11); bowl.cubicTo(4,23,20,23,21,11); bowl.closeSubpath(); p->drawPath(bowl);
            line(9,22,15,22); break;
        }
        case Icon::Fish:
            p->drawEllipse(QRectF(3,7,13,10)); path({{16,12},{21,7},{21,17},{16,12}}); p->drawPoint(QPointF(7,11)); break;
        case Icon::Plate:
            p->drawEllipse(QRectF(6,5,12,14)); p->drawEllipse(QRectF(9,8,6,8)); line(3,4,3,20); line(21,4,21,20); break;
        }
        p->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap result(size); result.fill(Qt::transparent); QPainter painter(&result);
        paint(&painter, QRect(QPoint(), size), mode, state); return result;
    }
private:
    Icon m_kind;
};
inline QIcon icon(Icon kind) { return QIcon(new LineIconEngine(kind)); }
inline Icon foodIcon(const QString &category) {
    if (category == QStringLiteral("荤菜")) return Icon::Meat;
    if (category == QStringLiteral("素菜")) return Icon::Vegetable;
    if (category == QStringLiteral("汤类")) return Icon::Soup;
    if (category == QStringLiteral("主食")) return Icon::Rice;
    if (category == QStringLiteral("水产")) return Icon::Fish;
    return Icon::Plate;
}
inline QPixmap foodPixmap(const QString &category, int size, qreal dpr) {
    QPixmap result(qRound(size*dpr), qRound(size*dpr)); result.setDevicePixelRatio(dpr); result.fill(Qt::transparent);
    QPainter painter(&result); LineIconEngine engine(foodIcon(category));
    engine.paint(&painter, QRect(0,0,size,size), QIcon::Normal, QIcon::Off); return result;
}
} // namespace Shixu::Ui
