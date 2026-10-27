
#pragma once

#include <QString>
#include "model/FoodCandidate.h"
#include "model/RecognitionResult.h"

class RecognitionService
{
public:
    // 小票 OCR 识别
    RecognitionResult recognizeReceipt(
        const QString& imagePath
    ) const;

    // 食材信息标准化
    FoodCandidate standardizeRecognitionResult(
        const FoodCandidate& input
    ) const;
};
