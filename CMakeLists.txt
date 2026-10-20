cmake_minimum_required(VERSION 3.20)

project(Shixu
    VERSION 0.1.0
    LANGUAGES CXX
)

# 使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Qt 配置：可选。主 Shixu 目标和 D 模块的演示需要 Qt Widgets；
# C 模块的 RecipeDemo 完全不依赖 Qt，可以在没有 Qt 的机器上单独构建。
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 OPTIONAL_COMPONENTS Widgets)
if(QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
    message(STATUS "Shixu: 检测到 Qt ${QT_VERSION}")
else()
    message(WARNING "Shixu: 未检测到 Qt Widgets。主程序 Shixu 和 RecommendationDemo 将被跳过；C 模块的 RecipeDemo 仍可构建。")
endif()

# C 模块源码（无 Qt 依赖，主程序和测试目标共用）。
set(SHIXU_RECIPE_SOURCES
    src/recipe/RecipeRepository.cpp
    src/recipe/RecipeData.cpp
    src/recipe/RecipeService.cpp
)

# ---------------------------------------------------------------------------
# 主程序入口：仅在 Qt 可用时构建。
# ---------------------------------------------------------------------------
if(QT_FOUND)
    add_executable(Shixu
        src/main.cpp
        ${SHIXU_RECIPE_SOURCES}
    )

    target_include_directories(Shixu
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(Shixu
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()

# ---------------------------------------------------------------------------
# C 模块独立演示程序：不依赖 Qt，直接验证 findRecipesByFoods。
# ---------------------------------------------------------------------------
add_executable(RecipeDemo
    tests/recipe/test_findRecipesByFoods.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(RecipeDemo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# C 模块阶段二独立验证程序：calculateMatch。
# ---------------------------------------------------------------------------
add_executable(CalculateMatchDemo
    tests/recipe/test_calculateMatch.cpp
    ${SHIXU_RECIPE_SOURCES}
)

target_include_directories(CalculateMatchDemo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
