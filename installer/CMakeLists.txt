cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

# So the exe runs on a PC without the Visual C++ runtime installed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The installer's own version, separate from any plugin it installs
project(SampleRealmInstaller VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)

set(CPM_DOWNLOAD_VERSION 0.43.1)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif ()

include(${CPM_DOWNLOAD_LOCATION})

# Same JUCE as the plugins
CPMAddPackage(
  NAME
    JUCE
  GIT_TAG
    9.0.2
  GITHUB_REPOSITORY
    juce-framework/JUCE
)

# Globs a source folder, keeping only this platform's *_mac / *_win files
function(platform_sources outVar folder)
    file(GLOB_RECURSE files CONFIGURE_DEPENDS
        "${folder}/*.cpp" "${folder}/*.h" "${folder}/*.mm")

    if(APPLE)
        list(FILTER files EXCLUDE REGEX "_win\\.cpp$")
    else()
        list(FILTER files EXCLUDE REGEX "_mac\\.(mm|cpp)$")
    endif()

    set(${outVar} ${files} PARENT_SCOPE)
endfunction()

# No UI, for reuse; an interface library because JUCE compiles its modules into every target
platform_sources(CoreFiles "${CMAKE_CURRENT_SOURCE_DIR}/Source/core")
add_library(InstallerCore INTERFACE)
target_sources(InstallerCore INTERFACE ${CoreFiles})
target_include_directories(InstallerCore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source/core")
target_link_libraries(InstallerCore INTERFACE juce::juce_core)

# Mac only: the app runs it as root, and it's a test CLI
# Links only juce_core, so UI code in core/ fails to build
if(APPLE)
    juce_add_console_app(InstallerHelper PRODUCT_NAME "InstallerHelper")
    target_sources(InstallerHelper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/helper/Main.cpp")
    target_link_libraries(InstallerHelper
        PRIVATE
            InstallerCore
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endif()

juce_add_gui_app(${PROJECT_NAME}
        PRODUCT_NAME
          SampleRealm-Installer
        COMPANY_NAME
          SampleRealm
        BUNDLE_ID
          com.SampleRealm.Installer
        VERSION
          ${PROJECT_VERSION}
)

platform_sources(AppFiles "${CMAKE_CURRENT_SOURCE_DIR}/Source/app")
target_sources(${PROJECT_NAME} PRIVATE ${AppFiles})

# Ships inside the app, which runs it as root. Its own target, so it runs every build and a
# helper-only change still reaches the app
if(APPLE)
    add_custom_target(HelperInApp ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Helpers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:InstallerHelper>" "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Helpers/"
        VERBATIM)
    add_dependencies(HelperInApp ${PROJECT_NAME} InstallerHelper)
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        InstallerCore
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
