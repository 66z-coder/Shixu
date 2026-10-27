#pragma once

#include <QString>
#include <QVector>
#include <QPolygonF>

// OCR 只读文字；不把商品价格、数量或到期日解释为食材。
struct ReceiptOcrLine
{
    QString text;
    double confidence = 0.0;
    QPolygonF box;
};

struct ReceiptOcrResult
{
    bool success = false;
    QVector<ReceiptOcrLine> lines;
    QString errorMessage;
};
