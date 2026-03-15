cmake_minimum_required(VERSION 3.20)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

file(GLOB QSCI_SOURCES "*.cpp")
file(GLOB QSCI_HEADERS "*.h" "Qsci/*.h")

# Scintilla core sources
file(GLOB SCINTILLA_SRC "../scintilla/src/*.cpp")
file(GLOB SCINTILLA_LEXLIB "../scintilla/lexlib/*.cpp")
file(GLOB SCINTILLA_LEXERS "../scintilla/lexers/*.cpp")

add_library(qscintilla STATIC
    ${QSCI_SOURCES}
    ${QSCI_HEADERS}
    ${SCINTILLA_SRC}
    ${SCINTILLA_LEXLIB}
    ${SCINTILLA_LEXERS}
)

target_include_directories(qscintilla PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Qsci
)

target_include_directories(qscintilla PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../scintilla/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../scintilla/lexlib
    ${CMAKE_CURRENT_SOURCE_DIR}/../scintilla/src
)

target_compile_definitions(qscintilla PRIVATE
    SCINTILLA_QT
    SCI_LEXER
    INCLUDE_DEPRECATED_FEATURES
)

target_link_libraries(qscintilla PUBLIC Qt6::Widgets Qt6::PrintSupport)

set_target_properties(qscintilla PROPERTIES AUTOMOC ON)

if(MSVC)
    target_compile_options(qscintilla PRIVATE /W0 /bigobj)
endif()
