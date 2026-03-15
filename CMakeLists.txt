cmake_minimum_required(VERSION 3.20)
project(NeonNote VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_subdirectory(third_party/qscintilla)

add_executable(NeonNote WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/TabBar.cpp
    src/TabBar.h
    src/BookmarkBar.cpp
    src/BookmarkBar.h
    src/StatusBar.cpp
    src/StatusBar.h
    src/EditorWidget.cpp
    src/EditorWidget.h
    src/AutoScrollHandler.cpp
    src/AutoScrollHandler.h
    src/FindReplaceBar.cpp
    src/FindReplaceBar.h
    src/SearchDialog.cpp
    src/SearchDialog.h
    src/SearchResultsPanel.cpp
    src/SearchResultsPanel.h
    src/Theme.h
)

target_include_directories(NeonNote PRIVATE src)

target_link_libraries(NeonNote PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    qscintilla
    dwmapi
)

if(MSVC)
    target_compile_options(NeonNote PRIVATE /W3)
    target_link_options(NeonNote PRIVATE /SUBSYSTEM:WINDOWS)
endif()
