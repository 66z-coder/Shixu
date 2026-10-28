cmake_minimum_required(VERSION 3.20)

project(Shixu VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

# C remains buildable without Qt.
add_library(ShixuRecipe STATIC
    src/recipe/RecipeRepository.cpp
    src/recipe/RecipeData.cpp
    src/recipe/MealRecordStorage.cpp
    src/recipe/RecipeService.cpp
)
target_include_directories(ShixuRecipe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)
    add_executable(ContractCompileTest tests/contracts/ContractCompileTest.cpp)
    target_link_libraries(ContractCompileTest PRIVATE ShixuRecipe)
    add_test(NAME ContractCompileTest COMMAND ContractCompileTest)
    add_executable(ContractImplementationTest tests/contracts/ContractImplementationTest.cpp)
    target_include_directories(ContractImplementationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME ContractImplementationTest COMMAND ContractImplementationTest)
    foreach(pair IN ITEMS "RecipeDemo|test_findRecipesByFoods" "CalculateMatchDemo|test_calculateMatch"
                          "CalculateNutritionDemo|test_calculateNutrition" "RecordMealDemo|test_recordMeal")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 target_name)
        list(GET parts 1 test_source)
        add_executable(${target_name} tests/recipe/${test_source}.cpp)
        target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/recipe)
        target_link_libraries(${target_name} PRIVATE ShixuRecipe)
        add_test(NAME ${target_name} COMMAND ${target_name})
    endforeach()
endif()

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)
if(QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    if(BUILD_TESTING)
        add_executable(QtContractCompileTest tests/contracts/QtContractCompileTest.cpp resources/recognition.qrc)
        target_include_directories(QtContractCompileTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/recipe)
        target_link_libraries(QtContractCompileTest PRIVATE ShixuRecipe ShixuRecognition Qt${QT_VERSION_MAJOR}::Core)
        add_test(NAME QtContractCompileTest COMMAND QtContractCompileTest)
    endif()

    # A: packaging-date OCR, manual entry and normalization.
    add_library(ShixuRecognition STATIC
        src/recognition/RecognitionService.cpp
        src/recognition/PackagingEntryWidget.cpp
        src/recognition/PackagingDateParser.cpp
        src/recognition/ReceiptOcrService.cpp
        include/recognition/PackagingEntryWidget.h
        include/recognition/ReceiptOcrService.h
    )
    target_include_directories(ShixuRecognition PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(ShixuRecognition PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ShixuRecognition PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

    add_executable(food_entry_demo tests/recognition/food_entry_demo.cpp resources/recognition.qrc)
    target_link_libraries(food_entry_demo PRIVATE ShixuRecognition)

    if(BUILD_TESTING)
        add_executable(recognition_test tests/recognition/recognition_test.cpp resources/recognition.qrc)
        target_link_libraries(recognition_test PRIVATE ShixuRecognition)
        add_test(NAME recognition_test COMMAND recognition_test)
        foreach(pair IN ITEMS "receipt_ocr_test|receipt_ocr_protocol" "packaging_date_test|packaging_date"
                              "packaging_entry_test|packaging_entry")
            string(REPLACE "|" ";" parts "${pair}")
            list(GET parts 0 target_name)
            list(GET parts 1 test_name)
            add_executable(${target_name} tests/recognition/${target_name}.cpp resources/recognition.qrc)
            target_link_libraries(${target_name} PRIVATE ShixuRecognition)
            target_compile_definitions(${target_name} PRIVATE SHIXU_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME ${test_name} COMMAND ${target_name})
        endforeach()
        set_tests_properties(packaging_entry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

    endif()

    add_library(ShixuRecommendation STATIC src/recommendation/RecommendationEngine.cpp)
    target_include_directories(ShixuRecommendation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(ShixuRecommendation PUBLIC ShixuRecipe Qt${QT_VERSION_MAJOR}::Core)

    # Retain E's application pages, reward system and resources.
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
        resources/recognition.qrc
        include/ui/InventoryWidget.h
        src/ui/InventoryWidget.cpp
    )
    target_include_directories(Shixu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(Shixu PRIVATE ShixuRecommendation ShixuRecognition Qt${QT_VERSION_MAJOR}::Widgets)

    add_executable(RecommendationDemo src/recommendation/demo_main.cpp)
    target_link_libraries(RecommendationDemo PRIVATE ShixuRecommendation Qt${QT_VERSION_MAJOR}::Widgets)

    if(BUILD_TESTING)
        add_executable(RecommendationTests tests/recommend/test_recommendation.cpp)
        target_link_libraries(RecommendationTests PRIVATE ShixuRecommendation)
        add_test(NAME RecommendationTests COMMAND RecommendationTests)

        add_executable(RecommendationWidgetTest
            tests/ui/RecommendationWidgetTest.cpp
            include/ui/RecommendationWidget.h
            src/ui/RecommendationWidget.cpp
        )
        target_include_directories(RecommendationWidgetTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(RecommendationWidgetTest PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
        add_test(NAME RecommendationWidgetTest COMMAND RecommendationWidgetTest)
        set_tests_properties(RecommendationWidgetTest PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    # Prefer B in the shared tree once merged. Until then, a caller may supply
    # B's checkout. The local ignored snapshot is only a development fallback.
    set(SHIXU_INVENTORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "B inventory module checkout")
    if(NOT EXISTS "${SHIXU_INVENTORY_ROOT}/src/inventory/inventory.cpp"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/inventory/src/inventory/inventory.cpp")
        set(SHIXU_INVENTORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/inventory")
    endif()
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Sql)
    if(TARGET Qt${QT_VERSION_MAJOR}::Sql
       AND EXISTS "${SHIXU_INVENTORY_ROOT}/src/inventory/inventory.cpp"
       AND EXISTS "${SHIXU_INVENTORY_ROOT}/src/inventory/inventory.h"
       AND EXISTS "${SHIXU_INVENTORY_ROOT}/include/model/FoodCandidate.h")
        message(STATUS "B inventory source: ${SHIXU_INVENTORY_ROOT}")
        add_library(ShixuInventory STATIC "${SHIXU_INVENTORY_ROOT}/src/inventory/inventory.cpp")
        target_include_directories(ShixuInventory PUBLIC
            "${SHIXU_INVENTORY_ROOT}/include" "${SHIXU_INVENTORY_ROOT}/src/inventory")
        target_link_libraries(ShixuInventory PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)

        target_link_libraries(Shixu PRIVATE ShixuInventory)

        # B 模块：库存管理独立测试
        add_executable(TestInventory
            test_inventory.cpp
            src/inventory/inventory.cpp
            src/inventory/inventory.h
            include/model/FoodCandidate.h
        )

        target_include_directories(TestInventory PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/inventory
        )

        target_link_libraries(TestInventory PRIVATE
            Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Sql
        )

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

        add_library(ShixuRecommendationInventory STATIC src/recommendation/RecommendationContextBuilder.cpp)
        target_link_libraries(ShixuRecommendationInventory PUBLIC ShixuRecommendation ShixuInventory)
        target_link_libraries(Shixu PRIVATE ShixuRecommendationInventory)
        if(BUILD_TESTING)
            get_target_property(SHIXU_APP_SOURCES Shixu SOURCES)
            list(REMOVE_ITEM SHIXU_APP_SOURCES src/main.cpp)
            add_executable(PackagingIntegrationTest tests/ui/PackagingIntegrationTest.cpp ${SHIXU_APP_SOURCES})
            target_include_directories(PackagingIntegrationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_link_libraries(PackagingIntegrationTest PRIVATE ShixuRecognition ShixuRecommendationInventory Qt${QT_VERSION_MAJOR}::Widgets)
            add_test(NAME PackagingIntegrationTest COMMAND PackagingIntegrationTest)
            set_tests_properties(PackagingIntegrationTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
        endif()


        add_executable(RecommendationLiveDemo src/recommendation/live_demo_main.cpp)
        target_link_libraries(RecommendationLiveDemo PRIVATE ShixuRecommendationInventory Qt${QT_VERSION_MAJOR}::Widgets)

        if(BUILD_TESTING)
            add_executable(RecommendationIntegrationTests tests/recommend/test_inventory_integration.cpp)
            target_link_libraries(RecommendationIntegrationTests PRIVATE ShixuRecommendationInventory)
            add_test(NAME RecommendationIntegrationTests COMMAND RecommendationIntegrationTests)
        endif()
    else()
        message(STATUS "B inventory or Qt Sql unavailable: live integration targets are skipped. Set SHIXU_INVENTORY_ROOT to B's checkout; see docs/D_recommendation_integration.md.")
    endif()
else()
    message(STATUS "Qt Widgets unavailable: only the C recipe library and tests are enabled.")
endif()
