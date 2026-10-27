cmake_minimum_required(VERSION 3.20)

project(Shixu
    VERSION 0.1.0
    LANGUAGES CXX
)

# 使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 为可选依赖：
# 检测到 Qt Widgets 时，构建主程序和 A 模块测试。
# 未检测到时，仍可构建不依赖 Qt 的 C 模块测试。
find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)

if(QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
    message(STATUS "Shixu: 检测到 Qt，启用主程序和 A 模块测试")
else()
    message(WARNING
        "Shixu: 未检测到 Qt Widgets，仅构建 C 模块测试"
    )
endif()

# C 模块：食谱与营养
set(SHIXU_RECIPE_SOURCES
    src/recipe/RecipeRepository.cpp
    src/recipe/RecipeData.cpp
    src/recipe/MealRecordStorage.cpp
    src/recipe/RecipeService.cpp
)

# A 模块：识别服务和食材录入界面
set(SHIXU_RECOGNITION_SOURCES
    src/recognition/RecognitionService.cpp
    src/recognition/FoodEntryWidget.cpp
    include/recognition/FoodEntryWidget.h
)

# ------------------------------------------------------------
# C 模块测试：不依赖 Qt
# ------------------------------------------------------------

# 阶段一：按食材查找食谱
add_executable(RecipeDemo
    tests/recipe/test_findRecipesByFoods.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(RecipeDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 阶段二：计算匹配程度
add_executable(CalculateMatchDemo
    tests/recipe/test_calculateMatch.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(CalculateMatchDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 阶段三：计算营养
add_executable(CalculateNutritionDemo
    tests/recipe/test_calculateNutrition.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(CalculateNutritionDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 阶段四：饮食记录与持久化
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
    # 仅为下面的 Qt 程序启用自动处理
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    # 主程序：包含 A、C 模块源码
    add_executable(Shixu
        src/main.cpp
        ${SHIXU_RECIPE_SOURCES}
        ${SHIXU_RECOGNITION_SOURCES}
    )

    target_include_directories(Shixu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(Shixu PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    )

    # A 模块：识别服务测试
    add_executable(recognition_test
        tests/recognition/recognition_test.cpp
        src/recognition/RecognitionService.cpp
    )

    target_include_directories(recognition_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(recognition_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    )

    # A 模块：食材录入界面演示
    add_executable(food_entry_demo
        tests/recognition/food_entry_demo.cpp
        ${SHIXU_RECOGNITION_SOURCES}
    )

    target_include_directories(food_entry_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(food_entry_demo PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()