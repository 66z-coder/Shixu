#pragma once
#include <QWidget>
#include <vector>
#include "contracts/CoreContracts.h"
class QLabel;
class QVBoxLayout;
class RecipeDetailsWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecipeDetailsWidget(QWidget* parent = nullptr);
    void setRecipe(const Recipe& recipe, const NutritionInfo& nutrition,
                   const std::vector<Shixu::Contracts::V1::IngredientAvailability>& availability);
signals:
    void cookingRequested(int recipeId);
private:
    int m_recipeId = 0;
    QLabel* m_heading = nullptr;
    QLabel* m_summary = nullptr;
    QVBoxLayout* m_ingredients = nullptr;
    QVBoxLayout* m_steps = nullptr;
};
