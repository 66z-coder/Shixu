#include "recognition/PackagingDateParser.h"
#include "recognition/RecognitionService.h"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    try {
        PackagingDateParser parser;
        auto r = parser.parse("有效期至：2027年2月28日");
        check(r.expiryDate == QDate(2027, 2, 28), "explicit expiry");
        r = parser.parse("生产日期：２０２６１０２０\n保质期：30天");
        check(r.productionDate == QDate(2026,10,20) && r.shelfLife == 30, "fullwidth compact production and days");
        check(PackagingDateParser::calculateExpiry(r.productionDate,30,ShelfLifeUnit::Days,false) == QDate(2026,11,19), "add full days");
        check(PackagingDateParser::calculateExpiry(r.productionDate,30,ShelfLifeUnit::Days,true) == QDate(2026,11,18), "inclusive start choice");
        check(PackagingDateParser::calculateExpiry(QDate(2024,1,31),1,ShelfLifeUnit::Months,false) == QDate(2024,2,29), "calendar month end");
        check(PackagingDateParser::calculateExpiry(QDate(2024,2,29),1,ShelfLifeUnit::Years,false) == QDate(2025,2,28), "calendar year");
        check(!parser.parse("2026-10-20").productionDate.isValid(), "unlabeled date not assigned");
        check(!parser.parse("有效期至 26/10/20").expiryDate.isValid(), "two digit year not guessed");
        check(parser.parse("有效期至 2026-02-30").requiresManualResolution, "invalid date");
        check(parser.parse("有效期至 2027-01-01\n有效期至 2027-02-01").requiresManualResolution, "conflicting dates");
        check(!parser.parse("生产日期 2026-10-20").expiryDate.isValid(), "missing duration");
        check(!parser.parse("保质期 12个月").expiryDate.isValid(), "missing production");
        r = parser.parse("生产日期\n2026/10/20\n保质期 12个月");
        check(r.productionDate.isValid() && r.shelfLifeUnit == ShelfLifeUnit::Months, "split label and date");
        check(parser.parse("保质期 0天").requiresManualResolution, "zero shelf life");
        check(parser.parse("生产日期 2026-10-20 保质期 30天 有效期至 2027-02-01").requiresManualResolution, "expiry versus computed conflict");
        check(parser.parse("开封后保质期 3天；保质期 12个月").requiresManualResolution, "conditional duration");
        auto fragment = [](QString text, qreal x, qreal y, qreal width, qreal height) {
            ReceiptOcrLine line; line.text = text; line.confidence = 0.99;
            line.box = QPolygonF(QRectF(x, y, width, height)); return line;
        };
        const QVector<ReceiptOcrLine> scrambled{
            fragment("20260927", 1022, 711, 320, 87),
            fragment("：20270326", 973, 780, 330, 77),
            fragment("保质期至：", 822, 787, 190, 78),
            fragment("21:08", 1333, 719, 230, 82),
            fragment("生产日期：", 823, 723, 190, 75),
            fragment("2AGM09", 1352, 781, 180, 76)
        };
        const QString arranged = PackagingDateParser::textFromOcrLines(scrambled);
        check(arranged.count('\n') == 1, "neighboring production and expiry rows stay separate");
        check(arranged.startsWith("生产日期: 20260927 21:08"), "restore left-to-right order from boxes");
        check(arranged.contains("保质期至: 20270326"), "remove duplicated boundary colon");
        r = parser.parse(arranged);
        check(r.productionDate == QDate(2026,9,27) && r.expiryDate == QDate(2027,3,26)
              && !r.requiresManualResolution, "fragmented label and dates reach date parser");
        const auto unlabeled = PackagingDateParser::textFromOcrLines({fragment("20260927", 0, 0, 100, 20)});
        check(!parser.parse(unlabeled).productionDate.isValid(), "reordering never invents date labels");
        auto conflicts = scrambled; conflicts.push_back(fragment("有效期至 2028-01-01", 0, 1000, 250, 40));
        check(parser.parse(PackagingDateParser::textFromOcrLines(conflicts)).requiresManualResolution,
              "reordering retains conflicting dates for manual review");
        ReceiptOcrLine missing; missing.text = "生产日期：";
        check(PackagingDateParser::textFromOcrLines({missing, fragment("20260927", 0, 40, 100, 20)})
              == QString("生产日期：").normalized(QString::NormalizationForm_KC) + "\n20260927",
              "missing geometry preserves original order");
        RecognitionService service;
        check(service.normalizeFoodName(" 西红柿 ") == "番茄", "name alias");
        check(service.normalizeFoodName("马铃薯") == "土豆", "dictionary alias");
        check(service.normalizeFoodName("苹果汁") == "苹果汁", "no substring rename");
        check(service.normalizeUnit("ＫＧ") == "kg" && service.normalizeUnit("毫升") == "ml", "unit aliases");
        check(!service.isSupportedUnit("一些") && !service.isSupportedUnit(""), "unknown unit validation");
        FoodCandidate food; food.name="西红柿"; food.quantity=500; food.unit="克";
        food.expiryDate=QDate(2027,1,1);
        const auto normalized=service.standardizeRecognitionResult(food);
        check(normalized.quantity==500 && normalized.unit=="g" && normalized.expiryDate==food.expiryDate, "unit spelling does not rescale quantity");
        std::cout << "PASS: dates, incomplete/conflicting labels, calendar rules, aliases and units\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
