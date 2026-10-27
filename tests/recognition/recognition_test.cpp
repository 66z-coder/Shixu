
#include "recognition/RecognitionService.h"
#include <iostream>

int main()
{
    RecognitionService service;

    // 测试1：食材名称和单位标准化
    FoodCandidate input;
    input.name = QStringLiteral("  西红柿  ");
    input.quantity = 2;
    input.unit = QStringLiteral("公斤");

    FoodCandidate result =
        service.standardizeRecognitionResult(input);

    if (result.name != QStringLiteral("番茄") ||
        result.quantity != 2 ||
        result.unit != QStringLiteral("kg"))
    {
        std::cout << "Test 1 FAILED" << std::endl;
        return 1;
    }

    std::cout << "Test 1 PASSED" << std::endl;

    // 测试2：单位为空时使用默认单位
    FoodCandidate input2;
    input2.name = QStringLiteral(" 牛奶 ");
    input2.unit = QStringLiteral(" ");

    FoodCandidate result2 =
        service.standardizeRecognitionResult(input2);

    if (result2.name != QStringLiteral("牛奶") ||
        result2.unit != QStringLiteral("个"))
    {
        std::cout << "Test 2 FAILED" << std::endl;
        return 1;
    }

    std::cout << "Test 2 PASSED" << std::endl;
    std::cout << "ALL TESTS PASSED!" << std::endl;

    return 0;
}
