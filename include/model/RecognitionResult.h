
#pragma once

#include <QString>
#include <vector>
#include "model/FoodCandidate.h"

struct RecognitionResult
{
    bool success = false;              // 是否识别成功
    std::vector<FoodCandidate> foods;   // 识别出的食材
    QString errorMessage;              // 失败原因
};
