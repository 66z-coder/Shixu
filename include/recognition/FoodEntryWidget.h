
#pragma once

#include <QWidget>
#include <vector>

#include "model/FoodCandidate.h"
#include "recognition/RecognitionService.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QListWidget;
class QDateEdit;
class QCheckBox;

class FoodEntryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FoodEntryWidget(QWidget* parent = nullptr);

signals:
    // 用户确认后，把食材列表发送给其他模块
    void foodsConfirmed(
        const std::vector<FoodCandidate>& foods
    );

private:
    // 食材输入控件
    QLineEdit* nameEdit_ = nullptr;
    QDoubleSpinBox* quantityEdit_ = nullptr;
    QComboBox* unitEdit_ = nullptr;

    // 日期选择控件
    QDateEdit* expiryDateEdit_ = nullptr;
    QCheckBox* unknownExpiryCheck_ = nullptr;

    // 待确认食材列表
    QListWidget* foodList_ = nullptr;

    // 食材标准化服务
    RecognitionService service_;

    // 保存真正的食材数据
    std::vector<FoodCandidate> pendingFoods_;

    // 当前正在修改的食材行号
    // -1 表示没有正在编辑的食材
    int editingRow_ = -1;
};
