cmake_minimum_required(VERSION 3.16)
project(XiangYue VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt组件
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Pdf PdfWidgets)

# 设置Qt的MOC、UIC、RCC自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 源文件
set(SOURCES
    main.cpp
    mainwindow.cpp
    loginwindow.cpp
    searchresultwindow.cpp
    resourcedetailwindow.cpp
    reportmanagementwindow.cpp
)

set(HEADERS
    mainwindow.h
    loginwindow.h
    searchresultwindow.h
    resourcedetailwindow.h
    reportmanagementwindow.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接Qt库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Pdf
    Qt6::PdfWidgets
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
