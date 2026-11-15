find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Заголовки с Q_OBJECT обязаны быть в списке исходников (для AUTOMOC),
# иначе получим "undefined reference to vtable".
set(GUI_SOURCES
    cppFolder/main.cpp
    cppFolder/mainwindow.cpp
    hFolder/mainwindow.h
)

qt_add_executable(chessClubGui ${GUI_SOURCES})

target_include_directories(chessClubGui PRIVATE hFolder)
target_link_libraries(chessClubGui PRIVATE chessCore Qt6::Widgets)

set_target_properties(chessClubGui PROPERTIES
    WIN32_EXECUTABLE ON
)
