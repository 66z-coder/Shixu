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
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# 当前程序入口
add_executable(Shixu
    src/main.cpp
    src/recognition/RecognitionService.cpp
)

# 公共头文件目录
target_include_directories(Shixu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接 Qt Widgets
target_link_libraries(Shixu
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
)

# 成员A：识别模块测试程序
add_executable(recognition_test
    tests/recognition/recognition_test.cpp
    src/recognition/RecognitionService.cpp
)

target_include_directories(recognition_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(recognition_test
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
)


# 成员A：食材录入界面独立演示程序
add_executable(food_entry_demo
    tests/recognition/food_entry_demo.cpp
    src/recognition/FoodEntryWidget.cpp
    src/recognition/RecognitionService.cpp
    include/recognition/FoodEntryWidget.h
)

target_include_directories(food_entry_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(food_entry_demo
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
)
