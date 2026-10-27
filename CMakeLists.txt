cmake_minimum_required(VERSION 3.20)

project(Shixu
    VERSION 0.1.0
    LANGUAGES CXX
)

# 使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 自动处理 moc / ui / qrc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 同时兼容 Qt6 和 Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)

# 当前程序入口
add_executable(Shixu
    src/main.cpp
    include/ui/MainWindow.h
    src/ui/MainWindow.cpp
    include/reward/RewardSystem.h
    src/reward/RewardSystem.cpp
    include/ui/CheckInWidget.h
    src/ui/CheckInWidget.cpp
    include/ui/StatisticsWidget.h
    src/ui/StatisticsWidget.cpp
    include/ui/RecommendationWidget.h
    src/ui/RecommendationWidget.cpp
    include/ui/HomeWidget.h
    src/ui/HomeWidget.cpp
    resources/home.qrc
    include/model/FoodCandidate.h
    src/inventory/inventory.h
    src/inventory/inventory.cpp
    include/ui/InventoryWidget.h
    src/ui/InventoryWidget.cpp
)

include(CTest)
if(BUILD_TESTING)
    add_executable(RewardSystemTest
        tests/reward/RewardSystemTest.cpp
        include/reward/RewardSystem.h
        src/reward/RewardSystem.cpp
    )
    target_include_directories(RewardSystemTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(RewardSystemTest PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME RewardSystemTest COMMAND RewardSystemTest)

    add_executable(InventoryIntegrationTest
        tests/inventory/InventoryIntegrationTest.cpp
        src/inventory/inventory.cpp
        src/inventory/inventory.h
        include/model/FoodCandidate.h
    )
    target_include_directories(InventoryIntegrationTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(InventoryIntegrationTest PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
    )
    add_test(NAME InventoryIntegrationTest COMMAND InventoryIntegrationTest)
endif()

# 公共头文件目录
target_include_directories(Shixu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接 Qt Widgets
target_link_libraries(Shixu
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
)
