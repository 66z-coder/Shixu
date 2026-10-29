#pragma once

#include <QMainWindow>
#include <QVector>
#include <memory>

class QLabel;
class QPushButton;
class QStackedWidget;
class QWidget;
class QFrame;
class HomeWidget;
class InventoryWidget;
class InventoryManager;
struct FoodCandidate;

class MainWindow : public QMainWindow {
  public:
    enum class Page {
        Home,
        FoodEntry,
        Inventory,
        Recipes,
        Recommendations,
        CheckIn,
        Statistics,
        Profile,
        Count
    };

    explicit MainWindow(QWidget *parent = nullptr, const QString &rewardFile = {});
    ~MainWindow() override;

    // Replaces a placeholder when a feature widget is ready. The window owns widget.
    void setPage(Page page, QWidget *widget);
    void showPage(Page page);
    void addFood(const FoodCandidate &candidate);

  private:
    struct Services;
    std::unique_ptr<Services> m_services;
    void initializeV1(class RewardSystem *rewards, class RecommendationWidget *recommendations);
    void openRecipe(int recipeId);
    bool m_showRecipeSteps = false;
    void openCooking(int recipeId, const QString &eventId = {});
    void refreshPending();
    QStackedWidget *m_pages = nullptr;
    QLabel *m_pageTitle = nullptr;
    QFrame *m_content = nullptr;
    QVector<QPushButton *> m_navigationButtons;
    InventoryManager *m_inventory = nullptr;
    HomeWidget *m_home = nullptr;
    InventoryWidget *m_inventoryPage = nullptr;

    void refreshInventory();
};
