cmake_minimum_required(VERSION 3.19)
project(CalibSystemV1 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

# VISA C API (1)
set(VISA_INCLUDE_DIR
    "C:/Program Files/IVI Foundation/VISA/Win64/Include")
set(VISA_LIB
    "C:/Program Files/IVI Foundation/VISA/Win64/Lib_x64/msc/visa64.lib")

qt_add_executable(CalibSystemV1
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    user/visaconnection.h user/visaconnection.cpp
    user/e5080b_driver.h user/e5080b_driver.cpp
    user/enadriverv2.h user/enadriverv2.cpp
)

# Link Qt + VISA (nhớ đổi tên project) (2)
target_include_directories(CalibSystemV1 PRIVATE
    ${VISA_INCLUDE_DIR}
    src)

target_link_libraries(CalibSystemV1
    PRIVATE
        Qt::Core
        Qt::Widgets
        "${VISA_LIB}"
)

# ── Copy visa64.dll vào thư mục build sau khi compile ──────────────── (nhớ đổi tên project) (3)
add_custom_command(TARGET CalibSystemV1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Windows/System32/visa64.dll"
        "$<TARGET_FILE_DIR:CalibSystemV1>"
    COMMENT "Copying visa64.dll to output directory")

install(TARGETS CalibSystemV1
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET CalibSystemV1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
