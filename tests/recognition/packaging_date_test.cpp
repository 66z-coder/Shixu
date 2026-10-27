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
