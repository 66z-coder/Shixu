
#pragma once

#include <QString>
#include "model/FoodCandidate.h"

class RecognitionService
{
public:
    QString normalizeFoodName(const QString& text) const;
    QString normalizeUnit(const QString& text) const;
    bool isSupportedUnit(const QString& text) const;
    // 食材信息标准化
    FoodCandidate standardizeRecognitionResult(
        const FoodCandidate& input
    ) const;
};
