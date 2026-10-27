#include "recognition/PackagingDateParser.h"
#include <QRegularExpression>
#include <QSet>

namespace {
const QString datePattern = "(?:[12]\\d{3}\\s*[-/.年]\\s*\\d{1,2}\\s*[-/.月]\\s*\\d{1,2}日?|[12]\\d{7})(?!\\d)";
}

QDate PackagingDateParser::parseDate(const QString& input)
{
    QString text = input.normalized(QString::NormalizationForm_KC).trimmed();
    text.remove(QRegularExpression("\\s+"));
    const QRegularExpression separated("^([12]\\d{3})[-/.年](\\d{1,2})[-/.月](\\d{1,2})日?$");
    const auto match = separated.match(text);
    if (match.hasMatch()) return QDate(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt());
    if (QRegularExpression("^[12]\\d{7}$").match(text).hasMatch())
        return QDate(text.left(4).toInt(), text.mid(4, 2).toInt(), text.right(2).toInt());
    return {};
}

QDate PackagingDateParser::calculateExpiry(const QDate& production, int duration,
                                          ShelfLifeUnit unit, bool productionIsDayOne)
{
    if (!production.isValid() || duration <= 0 || duration > 10000) return {};
    QDate expiry;
    switch (unit) {
    case ShelfLifeUnit::Days: expiry = production.addDays(duration); break;
    case ShelfLifeUnit::Months: expiry = production.addMonths(duration); break;
    case ShelfLifeUnit::Years: expiry = production.addYears(duration); break;
    }
    if (productionIsDayOne) expiry = expiry.addDays(-1);
    return expiry.year() <= 9999 && expiry >= production ? expiry : QDate();
}

DateRecognitionResult PackagingDateParser::parse(const QString& input) const
{
    const QString text = input.normalized(QString::NormalizationForm_KC);
    DateRecognitionResult result;
    auto readDates = [&](const QString& labels, QDate& output, const QString& field) {
        QSet<QDate> dates;
        const QRegularExpression expression("(?:" + labels + ")\\s*[:：]?\\s*(" + datePattern + ")");
        auto matches = expression.globalMatch(text);
        while (matches.hasNext()) {
            const auto date = parseDate(matches.next().captured(1));
            if (!date.isValid()) {
                result.requiresManualResolution = true;
                result.messages << field + "包含无效日期，请对照包装补充";
            } else dates.insert(date);
        }
        if (dates.size() == 1) output = *dates.begin();
        if (dates.size() > 1) {
            result.requiresManualResolution = true;
            result.messages << "识别到多个不同的" + field + "，请手动确认";
        }
    };
    readDates("生产日期|制造日期|生产时间", result.productionDate, "生产日期");
    readDates("到期日期|到期日|有效期至|保质期至|失效日期|最佳食用期至", result.expiryDate, "到期日期");
    QSet<QString> periods;
    const QRegularExpression shelf("(?:保质期|保存期)\\s*[:：]?\\s*(\\d{1,5})\\s*(天|日|个月|月|年)");
    auto matches = shelf.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const int value = match.captured(1).toInt();
        const QString label = match.captured(2);
        const int unit = label == "年" ? 2 : (label == "月" || label == "个月" ? 1 : 0);
        if (value <= 0 || value > 10000) {
            result.requiresManualResolution = true;
            result.messages << "保质期数值无效，请手动补充";
        } else periods.insert(QString::number(value) + ":" + QString::number(unit));
    }
    if (periods.size() == 1) {
        const auto parts = periods.begin()->split(':');
        result.shelfLife = parts[0].toInt();
        result.shelfLifeUnit = static_cast<ShelfLifeUnit>(parts[1].toInt());
    } else if (periods.size() > 1) {
        result.requiresManualResolution = true;
        result.messages << "识别到多个保质期，可能涉及开封或不同保存条件，请手动确认";
    }
    if (text.contains("开封") || text.contains("解冻") || text.contains("冷藏") || text.contains("冷冻")) {
        result.requiresManualResolution = true;
        result.messages << "存在开封/保存条件，请核对适用的日期及保质期，不自动选择条件";
    }
    if (result.productionDate.isValid() && result.expiryDate.isValid()) {
        if (result.expiryDate < result.productionDate) {
            result.requiresManualResolution = true;
            result.messages << "到期日期早于生产日期，请核对";
        }
        if (result.shelfLife > 0) {
            const auto a = calculateExpiry(result.productionDate, result.shelfLife, result.shelfLifeUnit, false);
            const auto b = calculateExpiry(result.productionDate, result.shelfLife, result.shelfLifeUnit, true);
            if (result.expiryDate != a && result.expiryDate != b) {
                result.requiresManualResolution = true;
                result.messages << "标注到期日与生产日期加保质期不一致，请核对并选择采用的依据";
            }
        }
    }
    if (!result.expiryDate.isValid() && (!result.productionDate.isValid() || result.shelfLife == 0))
        result.messages << "信息不完整：请补充明确到期日，或完整的生产日期和保质期";
    if (!result.expiryDate.isValid() && !result.productionDate.isValid())
        result.messages << "只接受带含义标签的完整四位年份日期；无标签日期不会自动采用";
    return result;
}
