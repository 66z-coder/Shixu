#pragma once
#include <QApplication>
#include <QFontDatabase>
namespace Shixu {
inline void applyTheme() {
    QString family;
    for (const auto &candidate :
         {"Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC", "WenQuanYi Micro Hei"})
        if (QFontDatabase::families().contains(candidate)) {
            family = candidate;
            break;
        }
    QFont font(family);
    font.setPointSize(10);
    QApplication::setFont(font);
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget { color:#28485D; }
        QWidget#pageBody { background:#F3F8FC; }
        QListWidget::item { padding:5px; }
        QListWidget::item:selected { background:#DDEFFC; color:#245C7E; }
        QLabel { background:transparent; }
        QPushButton { background:#FFFFFF; border:1px solid #CCDFE9; border-radius:9px; padding:8px 14px; }
        QPushButton:hover { background:#EAF4FB; border-color:#7CB5D6; }
        QPushButton:checked, QPushButton[primary="true"] { background:#287BA8; color:white; border-color:#287BA8; }
        QPushButton:disabled { color:#8B9EA9; background:#F1F5F7; }
        QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QPlainTextEdit { background:white; border:1px solid #CEDFE8; border-radius:7px; padding:6px; selection-background-color:#CBE7F7; }
        QListWidget,QTableWidget { background:white; border:1px solid #DCE8EF; border-radius:10px; alternate-background-color:#F4F9FC; }
        QHeaderView::section { background:#E8F3FA; color:#315970; padding:10px; border:0; font-weight:600; }
        QScrollArea { border:0; background:transparent; }
        QScrollBar:vertical { background:#F0F5F8; width:10px; }
        QScrollBar::handle:vertical { background:#BBD3E1; border-radius:5px; min-height:30px; }
        QFrame[primaryCard="true"] { border:2px solid #7FBADD; background:#F5FBFF; border-radius:14px; }
        QLabel[muted="true"] { color:#6C8392; }
    )"));
}
} // namespace Shixu
