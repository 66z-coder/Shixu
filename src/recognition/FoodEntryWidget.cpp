
#include "recognition/FoodEntryWidget.h"

#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QDate>
#include <QAbstractItemView>

FoodEntryWidget::FoodEntryWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("食序 - 食材录入");
    resize(480, 480);

    // =====================================
    // 1. 创建食材输入控件
    // =====================================

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("请输入食材名称");

    quantityEdit_ = new QDoubleSpinBox(this);
    quantityEdit_->setRange(0.01, 9999);
    quantityEdit_->setDecimals(2);
    quantityEdit_->setValue(1);

    unitEdit_ = new QComboBox(this);
    unitEdit_->addItems({
        "个", "盒", "袋", "瓶", "kg", "g"
    });

    // =====================================
    // 2. 创建到期日期选择控件
    // =====================================

    expiryDateEdit_ = new QDateEdit(this);
    expiryDateEdit_->setCalendarPopup(true);
    expiryDateEdit_->setDisplayFormat("yyyy-MM-dd");
    expiryDateEdit_->setDate(
        QDate::currentDate().addDays(7)
    );

    unknownExpiryCheck_ =
        new QCheckBox("到期日期未知", this);

    // 默认日期未知
    unknownExpiryCheck_->setChecked(true);
    expiryDateEdit_->setEnabled(false);

    connect(
        unknownExpiryCheck_,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            expiryDateEdit_->setEnabled(!checked);
        }
    );

    // =====================================
    // 3. 创建表单布局
    // =====================================

    auto* form = new QFormLayout();

    form->addRow("食材名称：", nameEdit_);
    form->addRow("数量：", quantityEdit_);
    form->addRow("单位：", unitEdit_);
    form->addRow("到期日期：", expiryDateEdit_);
    form->addRow("", unknownExpiryCheck_);

    // =====================================
    // 4. 创建列表和按钮
    // =====================================

    foodList_ = new QListWidget(this);

    // 一次只允许选择一条食材
    foodList_->setSelectionMode(
        QAbstractItemView::SingleSelection
    );

    auto* addButton =
        new QPushButton("添加食材", this);

    auto* editButton =
        new QPushButton("保存修改", this);

    auto* deleteButton =
        new QPushButton("删除选中食材", this);

    auto* confirmButton =
        new QPushButton("确认提交", this);

    // =====================================
    // 5. 创建整体布局
    // =====================================

    auto* layout = new QVBoxLayout(this);

    layout->addLayout(form);
    layout->addWidget(addButton);
    layout->addWidget(foodList_);
    layout->addWidget(editButton);
    layout->addWidget(deleteButton);
    layout->addWidget(confirmButton);

    // =====================================
    // 6. 清空输入框的辅助函数
    // =====================================

    auto clearForm = [this]() {

        nameEdit_->clear();

        quantityEdit_->setValue(1);

        unitEdit_->setCurrentIndex(0);

        unknownExpiryCheck_->setChecked(true);

        expiryDateEdit_->setDate(
            QDate::currentDate().addDays(7)
        );
    };

    // =====================================
    // 7. 食材显示格式的辅助函数
    // =====================================

    auto formatFood = [](const FoodCandidate& food) {

        QString dateText =
            food.expiryDate.isValid()
                ? food.expiryDate.toString("yyyy-MM-dd")
                : QStringLiteral("未知");

        return QString("%1 × %2 %3 | 到期：%4")
            .arg(food.name)
            .arg(food.quantity)
            .arg(food.unit)
            .arg(dateText);
    };

    // =====================================
    // 8. 添加食材
    // =====================================

    connect(
        addButton,
        &QPushButton::clicked,
        this,
        [this, clearForm, formatFood]() {

            // 编辑过程中不允许添加新食材
            if (editingRow_ != -1) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "请先保存当前食材的修改"
                );

                return;
            }

            FoodCandidate food;

            food.name = nameEdit_->text();

            food.quantity = quantityEdit_->value();

            food.unit = unitEdit_->currentText();

            // 用户手动选择到期日期
            if (unknownExpiryCheck_->isChecked()) {

                food.expiryDate = QDate();

            } else {

                food.expiryDate = expiryDateEdit_->date();
            }

            // 标准化食材数据
            food =
                service_.standardizeRecognitionResult(food);

            // 校验食材名称
            if (food.name.isEmpty()) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "食材名称不能为空"
                );

                return;
            }

            // 校验数量
            if (food.quantity <= 0) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "食材数量必须大于0"
                );

                return;
            }

            // 添加到真正的数据列表
            pendingFoods_.push_back(food);

            // 添加到界面列表
            foodList_->addItem(formatFood(food));

            // 清空输入框
            clearForm();
        }
    );

    // =====================================
    // 9. 双击食材，进入编辑模式
    // =====================================

    connect(
        foodList_,
        &QListWidget::itemDoubleClicked,
        this,
        [this](QListWidgetItem* item) {

            int row = foodList_->row(item);

            // 检查行号
            if (row < 0 ||
                row >= static_cast<int>(
                    pendingFoods_.size()
                )) {
                return;
            }

            // 记录真正要编辑的食材
            editingRow_ = row;

            const FoodCandidate& food =
                pendingFoods_[row];

            // 加载名称
            nameEdit_->setText(food.name);

            // 加载数量
            quantityEdit_->setValue(food.quantity);

            // 加载单位
            int unitIndex =
                unitEdit_->findText(food.unit);

            if (unitIndex >= 0) {

                unitEdit_->setCurrentIndex(unitIndex);

            } else {

                unitEdit_->addItem(food.unit);

                unitEdit_->setCurrentText(food.unit);
            }

            // 加载日期
            bool unknown =
                !food.expiryDate.isValid();

            unknownExpiryCheck_->setChecked(unknown);

            if (!unknown) {

                expiryDateEdit_->setDate(
                    food.expiryDate
                );
            }
        }
    );

    // =====================================
    // 10. 保存食材修改
    // =====================================

    connect(
        editButton,
        &QPushButton::clicked,
        this,
        [this, clearForm, formatFood]() {

            // 使用之前双击时记录的行号
            int row = editingRow_;

            if (row < 0 ||
                row >= static_cast<int>(
                    pendingFoods_.size()
                ) ||
                row >= foodList_->count()) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "请先双击要修改的食材"
                );

                return;
            }

            // 读取修改后的数据
            FoodCandidate food;

            food.name = nameEdit_->text();

            food.quantity = quantityEdit_->value();

            food.unit = unitEdit_->currentText();

            if (unknownExpiryCheck_->isChecked()) {

                food.expiryDate = QDate();

            } else {

                food.expiryDate =
                    expiryDateEdit_->date();
            }

            // 标准化
            food =
                service_.standardizeRecognitionResult(food);

            // 校验名称
            if (food.name.isEmpty()) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "食材名称不能为空"
                );

                return;
            }

            // 校验数量
            if (food.quantity <= 0) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "食材数量必须大于0"
                );

                return;
            }

            // 更新真正的数据
            pendingFoods_[row] = food;

            // 更新界面中的对应食材
            foodList_->item(row)->setText(
                formatFood(food)
            );

            // 退出编辑状态
            editingRow_ = -1;

            // 清空输入框
            clearForm();

            // 取消列表选中状态
            foodList_->clearSelection();

            QMessageBox::information(
                this,
                "提示",
                "食材修改成功"
            );
        }
    );

    // =====================================
    // 11. 删除选中的食材
    // =====================================

    connect(
        deleteButton,
        &QPushButton::clicked,
        this,
        [this, clearForm]() {

            // 获取当前选中的食材
            auto selectedItems =
                foodList_->selectedItems();

            if (selectedItems.isEmpty()) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "请先选择要删除的食材"
                );

                return;
            }

            // 获取选中行号
            int row = foodList_->row(
                selectedItems.first()
            );

            if (row < 0 ||
                row >= static_cast<int>(
                    pendingFoods_.size()
                )) {

                QMessageBox::warning(
                    this,
                    "错误",
                    "食材列表数据不一致"
                );

                return;
            }

            // 如果删除的正是正在编辑的食材
            if (row == editingRow_) {

                editingRow_ = -1;

                clearForm();
            }

            // 如果删除的是编辑目标之前的食材
            else if (editingRow_ >= 0 &&
                     row < editingRow_) {

                --editingRow_;
            }

            // 删除真正的数据
            pendingFoods_.erase(
                pendingFoods_.begin() + row
            );

            // 删除界面中的食材
            delete foodList_->takeItem(row);

            // 清除选中状态
            foodList_->clearSelection();
        }
    );

    // =====================================
    // 12. 确认提交食材
    // =====================================

    connect(
        confirmButton,
        &QPushButton::clicked,
        this,
        [this, clearForm]() {

            // 正在编辑时不允许提交
            if (editingRow_ != -1) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "当前有正在修改的食材，请先保存修改"
                );

                return;
            }

            // 检查是否有食材
            if (pendingFoods_.empty()) {

                QMessageBox::warning(
                    this,
                    "提示",
                    "请先添加食材"
                );

                return;
            }

            // 再次校验全部食材
            for (const auto& food : pendingFoods_) {

                if (food.name.trimmed().isEmpty() ||
                    food.quantity <= 0 ||
                    food.unit.trimmed().isEmpty()) {

                    QMessageBox::warning(
                        this,
                        "错误",
                        "存在不合法的食材信息"
                    );

                    return;
                }
            }

            // 向成员E的主界面发送数据
            emit foodsConfirmed(pendingFoods_);

            // 清空待确认的数据
            pendingFoods_.clear();

            // 清空界面列表
            foodList_->clear();

            // 重置编辑状态
            editingRow_ = -1;

            // 重置输入框
            clearForm();
        }
    );
}
