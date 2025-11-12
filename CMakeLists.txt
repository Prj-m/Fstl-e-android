cmake_minimum_required(VERSION 3.16)

project(AndroidApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# Find required Qt packages for Android
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets
    OpenGL
    OpenGLWidgets
    Network
)

# Source files - note: this appears to be a desktop Qt app being ported
# For Android with QML, you may want to remove desktop-specific files
set(PROJECT_SOURCES
    main.cpp
    app.cpp
    axis.cpp
    backdrop.cpp
    canvas.cpp
    glmesh.cpp
    loader.cpp
    mesh.cpp
    shaderlightprefs.cpp
    speedmousedialog.cpp
    window.cpp
    qml.qrc
    qt/qt.qrc
    gl/gl.qrc
)

if(ANDROID)
    # Set minimum Android API level
    qt_add_executable(appAndroidApp ${PROJECT_SOURCES})
    # Android-specific properties
    set_target_properties(appAndroidApp PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 33
    )
    # Disable Qt Accessibility on Android to prevent OpenGL deadlock crashes
    target_compile_definitions(appAndroidApp PRIVATE QT_NO_ACCESSIBILITY)
    # Log Android architecture for debugging
    message(STATUS "Building for Android architecture: ${ANDROID_ABI}")
else()
    qt_add_executable(appAndroidApp ${PROJECT_SOURCES})
endif()

# Set target properties for deployment
set_target_properties(appAndroidApp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Link required Qt modules
target_link_libraries(appAndroidApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Network
)

include(GNUInstallDirs)
install(TARGETS appAndroidApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
