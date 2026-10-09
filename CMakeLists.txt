cmake_minimum_required(VERSION 3.16)

project(FstlViewer VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
find_package(ZLIB REQUIRED)

# Find required Qt packages for Android
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets
    OpenGL
    OpenGLWidgets
)

# Option to enable/disable 3MF support (requires Qt private headers)
option(ENABLE_3MF "Enable 3MF file support (requires Qt private headers)" ON)

# Option to enable/disable Open CASCADE-based STEP support
# This requires OpenCASCADE libraries to be available for the target
# platform and will significantly increase binary size when enabled.
# Enabled by default; if OCCT is not found, the build falls back to the
# internal STEP parser and prints a warning.
option(ENABLE_OCCT_STEP "Enable Open CASCADE-based STEP support (larger binary)" ON)
option(FSTL_REQUIRE_OCCT "Fail configuration if full STEP support is unavailable" OFF)
set(FSTL_OCCT_ROOT "" CACHE PATH "OCCT installation prefix for the target platform")
set(FSTL_ANDROID_TARGET_SDK 36 CACHE STRING "Android target SDK API level")
if(FSTL_REQUIRE_OCCT AND NOT ENABLE_OCCT_STEP)
    message(FATAL_ERROR "FSTL_REQUIRE_OCCT requires ENABLE_OCCT_STEP=ON")
endif()

if(ENABLE_3MF)
    message(STATUS "3MF support enabled - will attempt to use QtCore private headers")
else()
    message(STATUS "3MF support DISABLED - only STL files will be supported")
endif()

if(ANDROID)
    # The pinned Qt Android kit exposes new-intent delivery through this API.
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
endif()

# Source files organized in subdirectories
set(PROJECT_SOURCES
    # Core sources
    src/core/main.cpp
    src/core/app.cpp
    src/core/mesh.cpp
    src/core/loader.cpp
    include/core/app.h
    include/core/loader.h
    include/core/boundedzip.h
    include/core/fastfloat.h
    include/core/mesh.h
    include/core/vertex.h
    
    # Loader sources
    src/loaders/stepmeshloader.cpp
    include/loaders/stepmeshloader.h
    src/loaders/occtsteploader.cpp
    include/loaders/occtsteploader.h
    
    # UI sources
    src/ui/canvas.cpp
    src/ui/window.cpp
    src/ui/shaderlightprefs.cpp
    src/ui/speedmousedialog.cpp
    src/ui/backdropsettingsdialog.cpp
    include/ui/canvas.h
    include/ui/window.h
    include/ui/shaderlightprefs.h
    include/ui/hsvplane.h
    include/ui/speedmousedialog.h
    include/ui/backdropsettingsdialog.h
    
    # Rendering sources
    src/rendering/axis.cpp
    src/rendering/backdrop.cpp
    src/rendering/glmesh.cpp
    include/rendering/axis.h
    include/rendering/backdrop.h
    include/rendering/glmesh.h
    
    # Resources
    resources/qml.qrc
    resources/icons.qrc
    resources/shaders.qrc
    resources/licenses.qrc
)

if(ANDROID)
    # Every Qt kit sets ANDROID_ABI; additional ABIs are built by Qt as sub-projects.
    set(FSTL_ANDROID_ABIS "arm64-v8a" CACHE STRING "Android ABIs packaged into the app")
    message(STATUS "Building for Android ABI: ${ANDROID_ABI}")
    
    # Set minimum Android API level - use qt6_add_executable for Android
    qt6_add_executable(fstl_viewer MANUAL_FINALIZATION ${PROJECT_SOURCES})
    # NDK r27 needs both flags: LOAD alignment alone leaves a 4 KB RELRO end.
    target_link_options(fstl_viewer PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")
    
    # Android-specific properties
    set_target_properties(fstl_viewer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION ${FSTL_ANDROID_TARGET_SDK}
        QT_ANDROID_ABIS "${FSTL_ANDROID_ABIS}"
        QT_ANDROID_NO_COMPRESS_RESOURCES_ARS YES
    )
    
    # Disable Qt Accessibility on Android to prevent OpenGL deadlock crashes
    target_compile_definitions(fstl_viewer PRIVATE QT_NO_ACCESSIBILITY)
else()
    qt6_add_executable(fstl_viewer MANUAL_FINALIZATION ${PROJECT_SOURCES})
endif()

# Set target properties for deployment
set_target_properties(fstl_viewer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Add include directories for new structure
target_compile_definitions(fstl_viewer PRIVATE FSTLE_VERSION="${PROJECT_VERSION}-rc.7")

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
        ZLIB::ZLIB
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
)

# Optional: full STEP support via Open CASCADE (desktop or heavy Android build)
if(ENABLE_OCCT_STEP)
    if(ANDROID)
        # For Android, manually link OCCT libraries from android/libs
        # Multi-ABI builds pass one OCCT prefix per ABI, e.g. FSTL_OCCT_ROOT_armeabi-v7a.
        if(FSTL_OCCT_ROOT_${ANDROID_ABI})
            set(FSTL_OCCT_ROOT "${FSTL_OCCT_ROOT_${ANDROID_ABI}}")
        endif()
        if(FSTL_OCCT_ROOT)
            set(OCCT_LIB_DIR "${FSTL_OCCT_ROOT}/lib")
            set(OCCT_INCLUDE_DIR "${FSTL_OCCT_ROOT}/include/opencascade")
        else()
            set(OCCT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/libs/${ANDROID_ABI}")
            set(OCCT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../occt-android/install-android-arm64-ndk/include/opencascade")
        endif()
        if(EXISTS "${OCCT_LIB_DIR}/libTKDESTEP.so" AND EXISTS "${OCCT_INCLUDE_DIR}/STEPControl_Reader.hxx")
            message(STATUS "Linking OCCT libraries from ${OCCT_LIB_DIR}")
            target_compile_definitions(fstl_viewer PRIVATE FSTL_USE_OCCT_STEP)
            target_include_directories(fstl_viewer PRIVATE "${OCCT_INCLUDE_DIR}")
            
            # Core OCCT libraries for STEP support
            target_link_libraries(fstl_viewer PRIVATE
                ${OCCT_LIB_DIR}/libTKDESTEP.so
                ${OCCT_LIB_DIR}/libTKXSBase.so
                ${OCCT_LIB_DIR}/libTKBRep.so
                ${OCCT_LIB_DIR}/libTKGeomBase.so
                ${OCCT_LIB_DIR}/libTKGeomAlgo.so
                ${OCCT_LIB_DIR}/libTKMesh.so
                ${OCCT_LIB_DIR}/libTKTopAlgo.so
                ${OCCT_LIB_DIR}/libTKG3d.so
                ${OCCT_LIB_DIR}/libTKG2d.so
                ${OCCT_LIB_DIR}/libTKMath.so
                ${OCCT_LIB_DIR}/libTKernel.so
            )
            # Include the complete OCCT dependency closure in the Android package.
            file(GLOB OCCT_ANDROID_LIBRARIES "${OCCT_LIB_DIR}/libTK*.so")
            set_property(TARGET fstl_viewer APPEND PROPERTY
                QT_ANDROID_EXTRA_LIBS ${OCCT_ANDROID_LIBRARIES})
            set(FSTL_OCCT_ENABLED TRUE)
        else()
            message(WARNING "OCCT libraries not found in ${OCCT_LIB_DIR} - using internal STEP parser")
        endif()
    else()
        # Desktop: use find_package
        if(FSTL_OCCT_ROOT)
            list(PREPEND CMAKE_PREFIX_PATH "${FSTL_OCCT_ROOT}")
        endif()

        find_package(OpenCASCADE QUIET)
        if(OpenCASCADE_FOUND)
            message(STATUS "OpenCASCADE found - enabling full STEP support")
            target_compile_definitions(fstl_viewer PRIVATE FSTL_USE_OCCT_STEP)
            target_include_directories(fstl_viewer PRIVATE ${OpenCASCADE_INCLUDE_DIR})
            target_link_libraries(fstl_viewer PRIVATE
                TKDESTEP
                TKXSBase
                TKBRep
                TKGeomBase
                TKGeomAlgo
                TKMesh
                TKTopAlgo
            )
            set(FSTL_OCCT_ENABLED TRUE)
        else()
            message(WARNING "ENABLE_OCCT_STEP=ON but OpenCASCADE not found - falling back to internal STEP parser")
        endif()
    endif()
endif()
if(FSTL_REQUIRE_OCCT AND NOT FSTL_OCCT_ENABLED)
    message(FATAL_ERROR "Full STEP support is required. Set FSTL_OCCT_ROOT to the target OCCT installation.")
endif()

# Link Android log library for logging
if(ANDROID)
    find_library(log-lib log)
    target_link_libraries(fstl_viewer PRIVATE ${log-lib} Qt6::CorePrivate)
endif()

# Resolve deployment dependencies after every library and property is set.
qt_finalize_executable(fstl_viewer)

include(GNUInstallDirs)
install(TARGETS fstl_viewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
