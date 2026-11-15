cmake_minimum_required(VERSION 3.21)
project(chessClub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------
#  Ядро: доменная логика без Qt. Используется и консолью, и GUI.
#  main.cpp исключён: у консоли и GUI свои точки входа.
# ---------------------------------------------------------------
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cppFolder/*.cpp")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cppFolder/main.cpp")

add_library(chessCore STATIC ${CORE_SOURCES})
target_include_directories(chessCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/hFolder")

# ---------------------------------------------------------------
#  Консольная версия
# ---------------------------------------------------------------
add_executable(chessClub cppFolder/main.cpp)
target_link_libraries(chessClub PRIVATE chessCore)

# ---------------------------------------------------------------
#  GUI на Qt Widgets
# ---------------------------------------------------------------
add_subdirectory(gui)
