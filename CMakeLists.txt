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