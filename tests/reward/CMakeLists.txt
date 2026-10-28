cmake_minimum_required(VERSION 3.20)
project(ShixuRewardV1Tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
add_executable(RewardSystemV1Test
    RewardContractTest.cpp
    ../../include/reward/RewardSystem.h
    ../../src/reward/RewardSystem.cpp
    ../../src/reward/RewardContractAdapter.cpp)
target_include_directories(RewardSystemV1Test PRIVATE ../../include)
target_link_libraries(RewardSystemV1Test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
enable_testing()
add_test(NAME RewardSystemV1Test COMMAND RewardSystemV1Test)