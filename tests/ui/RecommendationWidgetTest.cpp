#include "ui/RecommendationWidget.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    RecommendationWidget widget;
    widget.show();

    int requests = 0;
    Shixu::RecommendationEngine::Mood selected =
        Shixu::RecommendationEngine::Mood::NotSelected;
    int customLimit = -1;
    QObject::connect(&widget, &RecommendationWidget::recommendationRequested,
                     [&](Shixu::RecommendationEngine::Mood mood, int minutes) {
        ++requests;
        selected = mood;
        customLimit = minutes;
    });

    auto* generate = widget.findChild<QPushButton*>(QStringLiteral("generateRecommendations"));
    auto* hint = widget.findChild<QLabel*>(QStringLiteral("selectionHint"));
    auto* limit = widget.findChild<QSpinBox*>(QStringLiteral("customTimeLimit"));
    if (!check(generate && hint && limit, "Recommendation controls are missing")) {
        return 1;
    }

    generate->click();
    if (!check(requests == 0 && !hint->isHidden(),
               "A missing mood must show a prompt and block recommendation")) {
        return 1;
    }

    QPushButton* neutral = nullptr;
    for (QPushButton* button : widget.findChildren<QPushButton*>(QStringLiteral("moodChoice"))) {
        if (button->text().contains(QStringLiteral("状态一般"))) {
            neutral = button;
            break;
        }
    }
    if (!check(neutral != nullptr, "Neutral mood option is missing")) {
        return 1;
    }
    neutral->click();
    limit->setValue(20);
    generate->click();
    if (!check(requests == 1
                   && selected == Shixu::RecommendationEngine::Mood::Neutral
                   && customLimit == 20,
               "Selected mood and custom time cap must be passed to the integration layer")) {
        return 1;
    }

    RecommendationViewItem item;
    item.dishName = QStringLiteral("番茄炒蛋");
    item.moodName = QStringLiteral("状态一般");
    item.cookingTimeMinutes = 15;
    item.effectiveMaxPreparationMinutes = 20;
    item.moodMatchScore = 100;
    item.preparationTimeScore = 75;
    item.preparationTimeReferenceMinutes = 20;
    widget.setRecommendations({item});
    int moodRows = 0;
    int timeRows = 0;
    int oldPreferenceRows = 0;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
        moodRows += label->text() == QStringLiteral("心情匹配");
        timeRows += label->text() == QStringLiteral("制作时间");
        oldPreferenceRows += label->text() == QStringLiteral("口味偏好");
    }
    if (!check(moodRows == 1 && timeRows == 1 && oldPreferenceRows == 0,
               "Result must show separate mood and one time score, without taste preference")) {
        return 1;
    }
    return 0;
}
