#pragma once
#include <QApplication>
#include "ui/WheelGuard.h"
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QPushButton>
namespace Shixu {
inline void markPrimary(QPushButton *button) {
    button->setProperty("primary", true);
    // DialogButtonBox may already have polished a button before its role is assigned.
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->updateGeometry();
}
inline void applyTheme() {
    installWheelGuard();
    // Stable widget primitives avoid native Windows borders leaking through QSS.
    if (qApp->style()->objectName() != QStringLiteral("fusion"))
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, QColor("#EDF7F9"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, QColor("#28485D"));
    palette.setColor(QPalette::WindowText, QColor("#28485D"));
    palette.setColor(QPalette::Highlight, QColor("#287BA8"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#718A9A"));
    QApplication::setPalette(palette);
    QString family;
    for (const auto &candidate :
         {"Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC", "WenQuanYi Micro Hei"})
        if (QFontDatabase::families().contains(candidate)) {
            family = candidate;
            break;
        }
    QFont font(family);
    font.setPixelSize(14);
    QApplication::setFont(font);
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget { color:#28485D; }
        QWidget#pageBody, QDialog { background:#EDF7F9; }
        QLabel { background:transparent; }
        QLabel[muted="true"] { color:#627E90; font-size:12px; }
        QLabel[heading="true"] { color:#244E6A; font-size:18px; font-weight:700; }
        QFrame[card="true"] { background:white; border:1px solid #DCE8EF; border-radius:14px; }
        QPushButton { background:white; color:#315970; border:1px solid #CCDFE9; border-radius:10px; padding:8px 14px; min-height:20px; }
        QPushButton:hover { background:#EEF7FB; border-color:#7CB5D6; }
        QPushButton:pressed { background:#DCEEF8; border-color:#287BA8; }
        QPushButton:focus { border-color:#287BA8; }
        QPushButton:checked { background:#DCEEF8; border:2px solid #62A9D3; color:#185D88; font-weight:600; padding:7px 13px; }
        QPushButton:checked:hover { background:#CEE7F6; }
        QPushButton[primary="true"] { background:#287BA8; color:white; border-color:#287BA8; font-weight:600; min-height:26px; }
        QPushButton[primary="true"]:hover { background:#216B94; border-color:#216B94; }
        QPushButton[primary="true"]:pressed { background:#185D88; }
        QPushButton[primary="true"]:focus { border:2px solid #173F5E; padding:7px 13px; }
        QPushButton:disabled, QPushButton[primary="true"]:disabled { color:#8A9FAA; background:#F1F5F7; border-color:#DFE9EF; }
        QPushButton:checked:disabled { background:#DCEEF8; border-color:#62A9D3; color:#527D94; }
        QPushButton[link="true"] { background:transparent; border:1px solid transparent; color:#287BA8; padding:3px 0; min-height:18px; text-align:left; }
        QPushButton[link="true"]:hover { color:#185D88; text-decoration:underline; }
        QPushButton[link="true"]:focus { border-color:#7CB5D6; }
        QPushButton[danger="true"] { color:#A34B3D; border-color:#E7CDC7; }
        QPushButton[danger="true"]:hover { background:#FFF2EE; }
        QLineEdit,QComboBox,QAbstractSpinBox,QPlainTextEdit { background:white; border:1px solid #CEDFE8; border-radius:8px; padding:7px 10px; selection-background-color:#CBE7F7; selection-color:#17416B; }
        QLineEdit,QComboBox,QAbstractSpinBox { min-height:22px; }
        QLineEdit:hover,QComboBox:hover,QAbstractSpinBox:hover,QPlainTextEdit:hover { border-color:#A0C3D7; }
        QLineEdit:focus,QComboBox:focus,QAbstractSpinBox:focus,QPlainTextEdit:focus { border-color:#287BA8; }
        QLineEdit:disabled,QComboBox:disabled,QAbstractSpinBox:disabled { background:#F1F5F7; color:#8A9FAA; border-color:#DFE9EF; }
        QComboBox QLineEdit,QAbstractSpinBox QLineEdit { border:0; padding:0; min-height:0; background:transparent; }
        QComboBox { padding-right:32px; }
        QComboBox::drop-down { subcontrol-origin:border; subcontrol-position:top right; width:30px; border:0; background:transparent; }
        QComboBox::down-arrow { image:url(:/icons/chevron-down.png); width:14px; height:14px; }
        QComboBox QAbstractItemView { background:white; border:1px solid #CCDFE9; padding:4px; selection-background-color:#DCEEF8; selection-color:#185D88; outline:0; }
        QAbstractSpinBox { padding-right:30px; }
        QAbstractSpinBox::up-button,QAbstractSpinBox::down-button { width:25px; border:0; background:transparent; }
        QAbstractSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; }
        QAbstractSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; }
        QAbstractSpinBox::up-button:hover,QAbstractSpinBox::down-button:hover { background:#EAF4FA; }
        QSpinBox::up-arrow,QDoubleSpinBox::up-arrow { image:url(:/icons/chevron-up.png); width:12px; height:12px; }
        QSpinBox::down-arrow,QDoubleSpinBox::down-arrow { image:url(:/icons/chevron-down.png); width:12px; height:12px; }
        QDateEdit::drop-down { width:30px; border:0; background:transparent; }
        QDateEdit::down-arrow { image:url(:/icons/chevron-down.png); width:14px; height:14px; }
        QCheckBox { spacing:8px; padding:4px 0; }
        QCheckBox::indicator { width:16px; height:16px; background:white; border:1px solid #8FAFBE; border-radius:4px; }
        QCheckBox::indicator:hover { border-color:#287BA8; background:#EEF7FB; }
        QCheckBox::indicator:checked { background:#287BA8; border-color:#287BA8; image:url(:/icons/check.png); }
        QCheckBox::indicator:disabled { background:#F1F5F7; border-color:#CFDFE7; }
        QCheckBox::indicator:checked:disabled { background:#91B3C6; image:url(:/icons/check.png); }
        QCheckBox:disabled { color:#8A9FAA; }
        QListWidget,QTableWidget { background:white; border:1px solid #DCE8EF; border-radius:14px; alternate-background-color:#F5F9FC; outline:0; selection-background-color:#DCEEF8; selection-color:#185D88; }
        QListWidget { padding:6px; }
        QListWidget::item { padding:10px 8px; border-radius:8px; }
        QListWidget::item:hover { background:#EEF7FB; }
        QListWidget::item:selected,QTableWidget::item:selected { background:#DCEEF8; color:#185D88; }
        QListWidget::item:selected:hover { background:#CEE7F6; }
        QTableWidget::item { padding:8px; border-bottom:1px solid #EAF1F5; }
        QHeaderView::section { background:#E8F3FA; color:#315970; padding:12px 10px; border:0; font-weight:600; }
        QScrollArea { border:0; background:transparent; }
        QScrollArea > QWidget#qt_scrollarea_viewport { background:transparent; border:0; }
        QScrollArea[pageScroll="true"],QScrollArea[pageScroll="true"] > QWidget#qt_scrollarea_viewport { background:#EDF7F9; border:0; }
        QScrollBar:vertical { background:transparent; width:8px; margin:0; }
        QScrollBar::handle:vertical { background:#BBD3E1; border-radius:4px; min-height:32px; }
        QScrollBar::handle:vertical:hover { background:#8DB4CB; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; border:0; background:transparent; }
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }
        QScrollBar:horizontal { background:transparent; height:8px; margin:0; }
        QScrollBar::handle:horizontal { background:#BBD3E1; border-radius:4px; min-width:32px; }
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; border:0; background:transparent; }
        QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal { background:transparent; }
        QToolTip { color:#28485D; background:#FFFFFF; border:1px solid #CCDFE9; padding:6px; }
    )"));
}
} // namespace Shixu
