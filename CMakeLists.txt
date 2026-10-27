cmake_minimum_required(VERSION 3.20)

project(Shixu
    VERSION 0.1.0
    LANGUAGES CXX
)

# 使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Qt 可选：没有 Qt 时仍可构建 C 模块
find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)

if(QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
    message(STATUS "Shixu: 检测到 Qt，启用主程序和识别模块")
else()
    message(WARNING "Shixu: 未检测到 Qt Widgets，仅构建 C 模块测试")
endif()

# C 模块：食谱与营养
set(SHIXU_RECIPE_SOURCES
    src/recipe/RecipeRepository.cpp
    src/recipe/RecipeData.cpp
    src/recipe/MealRecordStorage.cpp
    src/recipe/RecipeService.cpp
)

# A 模块：原有识别服务和食材录入界面
set(SHIXU_RECOGNITION_SOURCES
    src/recognition/RecognitionService.cpp
)

# A 模块：新增包装录入、日期解析和 OCR
set(PACKAGE_ENTRY_SOURCES
    src/recognition/PackagingEntryWidget.cpp
    src/recognition/PackagingDateParser.cpp
    src/recognition/ReceiptOcrService.cpp
    include/recognition/PackagingEntryWidget.h
    include/recognition/ReceiptOcrService.h
)

# ------------------------------------------------------------
# C 模块测试：不依赖 Qt
# ------------------------------------------------------------

add_executable(RecipeDemo
    tests/recipe/test_findRecipesByFoods.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(RecipeDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(CalculateMatchDemo
    tests/recipe/test_calculateMatch.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(CalculateMatchDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(CalculateNutritionDemo
    tests/recipe/test_calculateNutrition.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(CalculateNutritionDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(RecordMealDemo
    tests/recipe/test_recordMeal.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(RecordMealDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recipe
)

# ------------------------------------------------------------
# 需要 Qt 的程序
# ------------------------------------------------------------

if(QT_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    # 主程序：保留 A、C，并加入 OCR 和包装日期功能
    add_executable(Shixu
        src/main.cpp
        ${SHIXU_RECIPE_SOURCES}
        ${SHIXU_RECOGNITION_SOURCES}
        ${PACKAGE_ENTRY_SOURCES}
    )

    # 识别服务测试
    add_executable(recognition_test
        tests/recognition/recognition_test.cpp
        src/recognition/RecognitionService.cpp
    )

    # 录入界面演示：同时保留原有界面和新增包装录入
    add_executable(food_entry_demo
        tests/recognition/food_entry_demo.cpp
        ${SHIXU_RECOGNITION_SOURCES}
        ${PACKAGE_ENTRY_SOURCES}
    )

    # OCR 协议测试
    add_executable(receipt_ocr_test
        tests/recognition/receipt_ocr_test.cpp
        src/recognition/ReceiptOcrService.cpp
        include/recognition/ReceiptOcrService.h
    )

    # 包装日期解析测试
    add_executable(packaging_date_test
        tests/recognition/packaging_date_test.cpp
        src/recognition/PackagingDateParser.cpp
        src/recognition/RecognitionService.cpp
    )

    # 包装录入界面测试
    add_executable(packaging_entry_test
        tests/recognition/packaging_entry_test.cpp
        src/recognition/RecognitionService.cpp
        ${PACKAGE_ENTRY_SOURCES}
    )

    # 这些目标共用头文件目录和 Qt Widgets
    foreach(target IN ITEMS
        Shixu
        recognition_test
        food_entry_demo
        receipt_ocr_test
        packaging_date_test
        packaging_entry_test
    )
        target_include_directories(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        target_link_libraries(${target} PRIVATE
            Qt${QT_VERSION_MAJOR}::Widgets
        )
    endforeach()

    # 保留 OCR 所需的项目目录宏
    foreach(target IN ITEMS
        Shixu
        food_entry_demo
        receipt_ocr_test
        packaging_entry_test
    )
        target_compile_definitions(${target} PRIVATE
            SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()

    # 保留本地版本的识别资源配置
    foreach(target IN ITEMS
        Shixu
        food_entry_demo
        recognition_test
        packaging_date_test
        packaging_entry_test
    )
        target_sources(${target} PRIVATE
            resources/recognition.qrc
        )
    endforeach()

    # 注册 A 模块测试
    add_test(
        NAME recognition_standardization
        COMMAND recognition_test
    )

    add_test(
        NAME receipt_ocr_protocol
        COMMAND receipt_ocr_test
    )

    add_test(
        NAME packaging_date
        COMMAND packaging_date_test
    )

    add_test(
        NAME packaging_entry
        COMMAND packaging_entry_test
    )

    set_tests_properties(packaging_entry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )
endif()