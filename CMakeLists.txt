cmake_minimum_required(VERSION 3.20)

project(Shixu
    VERSION 0.1.0
    LANGUAGES CXX
)

set(PACKAGE_ENTRY_SOURCES
    src/recognition/PackagingEntryWidget.cpp
    src/recognition/PackagingDateParser.cpp
    src/recognition/ReceiptOcrService.cpp
    include/recognition/PackagingEntryWidget.h
    include/recognition/ReceiptOcrService.h)

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
    ${PACKAGE_ENTRY_SOURCES}
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
    src/recognition/RecognitionService.cpp
    ${PACKAGE_ENTRY_SOURCES}
)

target_include_directories(food_entry_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(food_entry_demo
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(food_entry_demo PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

enable_testing()
add_test(NAME recognition_standardization COMMAND recognition_test)
add_executable(receipt_ocr_test
    tests/recognition/receipt_ocr_test.cpp
    src/recognition/ReceiptOcrService.cpp
    include/recognition/ReceiptOcrService.h)
target_include_directories(receipt_ocr_test PRIVATE include)
target_link_libraries(receipt_ocr_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_definitions(receipt_ocr_test PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME receipt_ocr_protocol COMMAND receipt_ocr_test)

target_compile_definitions(Shixu PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(packaging_date_test tests/recognition/packaging_date_test.cpp
    src/recognition/PackagingDateParser.cpp src/recognition/RecognitionService.cpp)
target_include_directories(packaging_date_test PRIVATE include)
target_link_libraries(packaging_date_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME packaging_date COMMAND packaging_date_test)

add_executable(packaging_entry_test tests/recognition/packaging_entry_test.cpp
    src/recognition/RecognitionService.cpp ${PACKAGE_ENTRY_SOURCES})
target_include_directories(packaging_entry_test PRIVATE include)
target_link_libraries(packaging_entry_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_definitions(packaging_entry_test PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME packaging_entry COMMAND packaging_entry_test)
set_tests_properties(packaging_entry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

foreach(target Shixu food_entry_demo recognition_test packaging_date_test packaging_entry_test)
    target_sources(${target} PRIVATE resources/recognition.qrc)
endforeach()
