cmake_minimum_required(VERSION 3.16)
project(LiniennetzplanEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Svg)

set(APP_SOURCES
    src/main.cpp
    src/l10n.cpp
    src/model.cpp
    src/geom.cpp
    src/layout.cpp
    src/render.cpp
    src/netview.cpp
    src/mainwindow.cpp
)
set(APP_HEADERS
    src/l10n.h
    src/model.h
    src/geom.h
    src/layout.h
    src/render.h
    src/netview.h
    src/mainwindow.h
)

add_executable(LiniennetzplanEditor ${APP_SOURCES} ${APP_HEADERS})
target_link_libraries(LiniennetzplanEditor PRIVATE Qt6::Widgets Qt6::Svg)

# Windows: Kein Konsolenfenster anzeigen (GUI-Anwendung)
if(WIN32 AND MINGW)
    set_target_properties(LiniennetzplanEditor PROPERTIES LINK_FLAGS "-mwindows")
endif()
