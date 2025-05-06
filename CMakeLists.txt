cmake_minimum_required(VERSION 3.16)
project(FaceAuthAccess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core Gui Widgets Network Multimedia
)

# 启用 AUTOMOC AUTOUIC 等
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 客户端源文件
set(CLIENT_SOURCES
    main.cpp
    FaceAuthClient.ui
    FaceAuthClient.h
    FaceAuthClient.cpp
    ServerSettingsDialog.h
    ServerSettingsDialog.cpp
)

# OpenCV 路径手动设置
set(OpenCV_INCLUDE_DIR "E:/qt_project/opencv/build/include")
set(OpenCV_LIB_DIR "E:/qt_project/opencv/build/x64/vc16/lib")

include_directories(${OpenCV_INCLUDE_DIR})
link_directories(${OpenCV_LIB_DIR})

# 客户端
add_executable(FaceAuthClient ${CLIENT_SOURCES})
target_link_libraries(FaceAuthClient PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Multimedia
    debug "${OpenCV_LIB_DIR}/opencv_world4110d.lib"
    optimized "${OpenCV_LIB_DIR}/opencv_world4110.lib"
)

# 添加包含目录
target_include_directories(FaceAuthClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
