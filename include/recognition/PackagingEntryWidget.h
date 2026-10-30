#pragma once
#include <QWidget>
#include <vector>
#include "model/FoodCandidate.h"
#include "recognition/ReceiptOcrService.h"
#include "recognition/RecognitionService.h"
#include "model/DateRecognitionResult.h"
class QDialog;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QListWidget;
class QFormLayout;

class PackagingEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackagingEntryWidget(QWidget* parent = nullptr);
    // Receiver must acknowledge each foodsConfirmed submission.
    void completeSubmission(bool success, const QString& message = QString());
    // Shared import path for file picker, local file drop and integration callers.
    bool loadImageFile(const QString& path);
signals:
    void foodsConfirmed(const std::vector<FoodCandidate>& foods);
private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void chooseImage();
    void parseText();
    void openDateRecognition();
    void applyRecognizedDates();
    void refresh();
    void clearForm();
    void editSelected();
    QString validate(FoodCandidate& candidate) const;
    QLineEdit *name_, *production_, *expiry_;
    QDoubleSpinBox *quantity_, *unitWeight_;
    QComboBox *unit_, *mode_, *periodUnit_, *category_, *netContentUnit_;
    QFormLayout *form_;
    QWidget *expiryRow_, *productionRow_, *netContentRow_;
    QDialog* recognitionDialog_;
    QLabel *recognitionStatus_, *recognitionSummary_;
    QPushButton *openRecognition_, *applyDates_, *directSave_;
    DateRecognitionResult recognizedDates_;
    bool recognitionActive_ = false;
    bool directSubmission_ = false;
    bool categoryManual_ = false;
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
    bool submitting_ = false;
    QString imagePath_;
    QDate storageStart_;
    ReceiptOcrService ocr_;
    RecognitionService normalizer_;
};
