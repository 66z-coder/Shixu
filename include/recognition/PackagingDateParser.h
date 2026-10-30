#pragma once
#include "model/DateRecognitionResult.h"
#include "model/ReceiptOcrResult.h"

class PackagingDateParser
{
public:
    DateRecognitionResult parse(const QString& text) const;
    // Restore reading order from OCR geometry before interpreting date labels.
    static QString textFromOcrLines(const QVector<ReceiptOcrLine>& lines);
    static QDate parseDate(const QString& text);
    // 起算约定必须由调用者明确传入；月份和年份使用日历运算。
    static QDate calculateExpiry(const QDate& production, int duration,
                                ShelfLifeUnit unit, bool productionIsDayOne);
};
