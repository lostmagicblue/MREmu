cmake_minimum_required (VERSION 3.8)

project(MREmu)

# 如果是 Windows 平台，强制将 MRE SDK 的 include 目录加入全局头文件搜索路径
if(WIN32)
    set(MRE_SDK_INC "C:/Program Files (x86)/MRE SDK V3.0.00/include")
    if(EXISTS "${MRE_SDK_INC}")
        message(STATUS "Found MRE SDK include dir at: ${MRE_SDK_INC}")
        include_directories("${MRE_SDK_INC}")
    else()
        message(WARNING "MRE SDK include dir not found at: ${MRE_SDK_INC}")
    endif()
endif()

# 先添加子目录，确保 MREngine 和 NativeApps 目标在被链接前已定义
add_subdirectory(MREngine)
add_subdirectory(NativeApps)

set(SOURCES 
    "MREmu.cpp" "MREmu.h" 
    "Memory.cpp" "Memory.h" 
    "Cpu.cpp" "Cpu.h" 
    "GDB.cpp" "GDB.h" 
    "Bridge.cpp" "Bridge.h" 
    "App.cpp" "App.h" 
    "ArmApp.cpp" "ArmApp.h" 
    "DLLApp.cpp" "DLLApp.h" 
    "NativeApp.cpp" "NativeApp.h" 
    "MreTags.cpp" "MreTags.h" 
    "Disassembler.cpp" "Disassembler.h"
    "ARModule.cpp" "ARModule.h"
    "AppManager.cpp" "AppManager.h"
    "Keyboard.cpp" "Keyboard.h"
    "Touch.cpp" "Touch.h"
    "Log.cpp" "Log.h" 
    "miniz.c" "miniz.h" "mutex_wrapper.h"
    "jni/Vibration.cpp" "jni/Vibration.h"
)

if (ANDROID)
    add_library(MREmu SHARED ${SOURCES})
else()
    add_executable(MREmu ${SOURCES})
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MREmu PRIVATE
    sfml-graphics
    sfml-network
    sfml-audio
    imgui
    unicorn
    ADLMIDI_static
    MREngine
    NativeApps
    libiconv
    spdlog
)

if(CAPSTONE)
    target_link_libraries(MREmu PRIVATE capstone)
endif()

if (ANDROID)
    target_link_libraries(MREmu PRIVATE Android -Wl,--whole-archive sfml-main -Wl,--no-whole-archive)

    add_library(AL SHARED IMPORTED) 
    set_target_properties(AL PROPERTIES IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/deps/SFML/extlibs/libs-android/${CMAKE_ANDROID_ARCH_ABI}/libopenal.so")
    target_link_libraries(MREmu PRIVATE AL)
endif()
