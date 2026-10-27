
#include "recognition/RecognitionService.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QJsonObject& mappings()
{
    static const QJsonObject data = [] {
        QFile file(":/recognition/normalization.json");
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return data;
}
}

QString RecognitionService::normalizeFoodName(const QString& text) const
{
    const QString name = text.normalized(QString::NormalizationForm_KC).simplified();
    return mappings().value("names").toObject().value(name).toString(name);
}

QString RecognitionService::normalizeUnit(const QString& text) const
{
    const QString unit = text.normalized(QString::NormalizationForm_KC).trimmed().toLower();
    return mappings().value("units").toObject().value(unit).toString(unit);
}

bool RecognitionService::isSupportedUnit(const QString& text) const
{
    return mappings().value("units").toObject().contains(
        text.normalized(QString::NormalizationForm_KC).trimmed().toLower());
}

// 食材信息标准化
FoodCandidate RecognitionService::standardizeRecognitionResult(
    const FoodCandidate& input
) const
{
    FoodCandidate result = input;

    // 1. 清理名称中多余的空格
    result.name = normalizeFoodName(result.name);

    // 2. 统一常见食材名称

    // 3. 处理单位
    result.unit = normalizeUnit(result.unit);

    if (result.unit.isEmpty()) {
        result.unit = "个";
    }


    // 数量和日期保持原样
    return result;
}
