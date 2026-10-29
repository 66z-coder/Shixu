cmake_minimum_required(VERSION 3.20)
project(ShixuUiV1Tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
add_executable(UiContractTest
    UiContractTest.cpp
    ../../include/ui/RecommendationWidget.h ../../src/ui/RecommendationWidget.cpp
    ../../include/ui/RecipeDetailsWidget.h ../../src/ui/RecipeDetailsWidget.cpp
    ../../include/ui/CookingConfirmationWidget.h ../../src/ui/CookingConfirmationWidget.cpp)
target_include_directories(UiContractTest PRIVATE ../../include)
target_link_libraries(UiContractTest PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
enable_testing()
add_test(NAME UiContractTest COMMAND UiContractTest)
add_executable(LegacyRoutingTest
    LegacyRoutingTest.cpp
    ../../include/reward/RewardSystem.h ../../src/reward/RewardSystem.cpp
    ../../include/ui/HomeWidget.h ../../src/ui/HomeWidget.cpp
    ../../include/ui/RecommendationWidget.h ../../src/ui/RecommendationWidget.cpp)
target_include_directories(LegacyRoutingTest PRIVATE ../../include)
target_link_libraries(LegacyRoutingTest PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME LegacyRoutingTest COMMAND LegacyRoutingTest)
