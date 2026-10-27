#pragma once
#include <QWidget>
#include <vector>
#include "model/FoodCandidate.h"
#include "recognition/ReceiptOcrService.h"
#include "recognition/RecognitionService.h"
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QListWidget;

class PackagingEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackagingEntryWidget(QWidget* parent = nullptr);
signals:
    void foodsConfirmed(const std::vector<FoodCandidate>& foods);
private:
    void chooseImage();
    void parseText();
    void refresh();
    void clearForm();
    void editSelected();
    QString validate(FoodCandidate& candidate) const;
    QLineEdit *name_, *production_, *expiry_;
    QDoubleSpinBox *quantity_, *unitWeight_;
    QComboBox *unit_, *mode_, *periodUnit_;
    QSpinBox *period_, *storageDays_;
    QPlainTextEdit* raw_;
    QCheckBox* confirmed_;
    QLabel *status_, *preview_, *computed_, *storageDaysLabel_, *unitWeightLabel_, *totalWeight_;
    QPushButton *choose_, *recognize_, *cancel_, *parse_, *submit_, *save_, *reset_, *edit_, *remove_;
    QListWidget* list_;
    struct Entry {
        FoodCandidate food;
        QString raw, production, expiry, imagePath;
        int mode = 0, period = 0, periodUnit = 0;
        int storageDays = 0;
        QDate storageStart;
    };
    std::vector<Entry> entries_;
    int editingRow_ = -1;
    QString imagePath_;
    QDate storageStart_;
    ReceiptOcrService ocr_;
    RecognitionService normalizer_;
};
