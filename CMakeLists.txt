cmake_minimum_required(VERSION 3.16)

project(FstlViewer VERSION 0.1 LANGUAGES CXX)

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
    Xml
)

# Option to enable/disable 3MF support (requires Qt private headers)
option(ENABLE_3MF "Enable 3MF file support (requires Qt private headers)" ON)

if(ENABLE_3MF)
    message(STATUS "3MF support enabled - will attempt to use QtCore private headers")
else()
    message(STATUS "3MF support DISABLED - only STL files will be supported")
endif()

# Source files organized in subdirectories
set(PROJECT_SOURCES
    # Core sources
    src/core/main.cpp
    src/core/app.cpp
    src/core/mesh.cpp
    src/core/loader.cpp
    
    # Loader sources
    src/loaders/stepmeshloader.cpp
    
    # UI sources
    src/ui/canvas.cpp
    src/ui/window.cpp
    src/ui/shaderlightprefs.cpp
    src/ui/speedmousedialog.cpp
    
    # Rendering sources
    src/rendering/axis.cpp
    src/rendering/backdrop.cpp
    src/rendering/glmesh.cpp
    
    # Resources
    resources/qml.qrc
    resources/icons.qrc
    resources/shaders.qrc
)

if(ANDROID)
    # Explicitly set the Android ABI if not already set
    if(NOT ANDROID_ABI)
        set(ANDROID_ABI "arm64-v8a")
    endif()
    message(STATUS "Building for Android ABI: ${ANDROID_ABI}")
    
    # Set minimum Android API level - use qt6_add_executable for Android
    qt6_add_executable(fstl_viewer ${PROJECT_SOURCES})
    
    # Android-specific properties
    set_target_properties(fstl_viewer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_ABIS "arm64-v8a"
    )
    
    # Disable Qt Accessibility on Android to prevent OpenGL deadlock crashes
    target_compile_definitions(fstl_viewer PRIVATE QT_NO_ACCESSIBILITY)
    # Log Android architecture for debugging
    message(STATUS "Building for Android architecture: ${ANDROID_ABI}")
else()
    qt6_add_executable(fstl_viewer ${PROJECT_SOURCES})
endif()

# Finalize the executable for all platforms
qt_finalize_executable(fstl_viewer)

# Set target properties for deployment
set_target_properties(fstl_viewer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Add include directories for new structure
target_include_directories(fstl_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Include Qt private headers for QZipReader
get_target_property(Qt6Core_INCLUDE_DIRS Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir ${Qt6Core_INCLUDE_DIRS})
    if(EXISTS "${dir}/QtCore")
        # Find Qt version-specific private headers dynamically
        file(GLOB QT_VERSION_DIRS "${dir}/QtCore/*/QtCore/private")
        foreach(private_dir ${QT_VERSION_DIRS})
            get_filename_component(version_dir "${private_dir}/../.." ABSOLUTE)
            target_include_directories(fstl_viewer PRIVATE "${private_dir}")
            target_include_directories(fstl_viewer PRIVATE "${version_dir}")
            message(STATUS "Found Qt private headers: ${private_dir}")
        endforeach()
    endif()
endforeach()

# Link required Qt modules
target_link_libraries(fstl_viewer
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Network
        Qt6::Xml
)

# Link Android log library for logging
if(ANDROID)
    find_library(log-lib log)
    target_link_libraries(fstl_viewer PRIVATE ${log-lib})
endif()


include(GNUInstallDirs)
install(TARGETS fstl_viewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
