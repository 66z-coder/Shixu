#pragma once
#include "contracts/CoreContracts.h"
#include <QWidget>
#include <QVector>
#include <vector>
class QLabel;
class QVBoxLayout;
class QFrame;
class QProgressBar;
class QScrollArea;
class RecipeDetailsWidget : public QWidget {
    Q_OBJECT
  public:
    explicit RecipeDetailsWidget(QWidget *parent = nullptr);
    void setRecipe(const Recipe &recipe, const NutritionInfo &nutrition,
                   const std::vector<Shixu::Contracts::V1::IngredientAvailability> &availability);
    void clearRecipe();
    void showSteps();
    void scrollToTop();
  signals:
    void cookingRequested(int recipeId);

  private:
    int m_recipeId = 0;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_heading = nullptr;
    QLabel *m_categoryIcon = nullptr;
    QLabel *m_summary = nullptr;
    QFrame *m_nutritionCard = nullptr;
    QLabel *m_calories = nullptr;
    QVector<QLabel *> m_macroValues;
    QVector<QProgressBar *> m_macroBars;
    QVBoxLayout *m_ingredients = nullptr;
    QVBoxLayout *m_steps = nullptr;
};
