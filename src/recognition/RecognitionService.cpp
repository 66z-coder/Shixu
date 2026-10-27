
#include "recognition/RecognitionService.h"

// 小票识别：暂时预留接口
RecognitionResult RecognitionService::recognizeReceipt(
    const QString& imagePath
) const
{
    Q_UNUSED(imagePath);

    RecognitionResult result;
    result.success = false;
    result.errorMessage = "小票 OCR 功能尚未接入";

    return result;
}

// 食材信息标准化
FoodCandidate RecognitionService::standardizeRecognitionResult(
    const FoodCandidate& input
) const
{
    FoodCandidate result = input;

    // 1. 清理名称中多余的空格
    result.name = result.name.simplified();

    // 2. 统一常见食材名称
    if (result.name == "西红柿") {
        result.name = "番茄";
    }

    // 3. 处理单位
    result.unit = result.unit.trimmed();

    if (result.unit.isEmpty()) {
        result.unit = "个";
    }

    if (result.unit == "公斤" ||
        result.unit == "千克") {
        result.unit = "kg";
    }

    // 数量和日期保持原样
    return result;
}
