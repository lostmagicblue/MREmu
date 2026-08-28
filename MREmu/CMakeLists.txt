cmake_minimum_required (VERSION 3.8)

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
						"UiLogSink.cpp" "UiLogSink.h"
						"miniz.c" "miniz.h" "mutex_wrapper.h"
						"jni/Vibration.cpp" "jni/Vibration.h"
)

if (ANDROID)
	add_library(MREmu SHARED ${SOURCES})
else()
	add_executable(MREmu ${SOURCES})
endif()

# —— 关键修复：Visual Studio / MSBuild 多配置生成器下，add_executable(${SOURCES}) 里如果
#    有在「首次 configure 之后新加入 set(SOURCES)」的文件，.vcxproj 不一定更新进去。
#    target_sources(MREmu PRIVATE …) 是强制把这两个编译单元绑定到 MREmu 目标，跨所有
#    生成器（VS/Ninja/Xcode）、跨多次 re-configure 都稳定生效。
target_sources(MREmu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/UiLogSink.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UiLogSink.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MREmu PROPERTY CXX_STANDARD 20)
endif()

# MSVC：禁用 SDK 头 vmsys.h 里的 GBK 非法字符告警（C4828 / C4819）。
# vmsys.h 是外部提供的 mre_sdk，不是我们的源码，不能改它的字节编码，
# 也不需要 /utf-8（/utf-8 反而会强制按 UTF-8 读它，问题更大），直接屏蔽警告即可。
if(MSVC)
    target_compile_options(MREmu PRIVATE /wd4828 /wd4819)
endif()

target_link_libraries(MREmu sfml-graphics)
target_link_libraries(MREmu sfml-network)
target_link_libraries(MREmu sfml-audio)
target_link_libraries(MREmu imgui)
target_link_libraries(MREmu unicorn)
target_link_libraries(MREmu ADLMIDI_static)
target_link_libraries(MREmu MREngine)
target_link_libraries(MREmu NativeApps)
target_link_libraries(MREmu libiconv)
target_link_libraries(MREmu spdlog)

if(CAPSTONE)
	target_link_libraries(MREmu capstone)
endif()

if (ANDROID)
	target_link_libraries(MREmu Android -Wl,--whole-archive sfml-main -Wl,--no-whole-archive)

	add_library(AL SHARED IMPORTED) 
	set_target_properties(AL PROPERTIES IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/deps/SFML/extlibs/libs-android/${CMAKE_ANDROID_ARCH_ABI}/libopenal.so")
	target_link_libraries(MREmu AL)
endif()

add_subdirectory (MREngine)
add_subdirectory (NativeApps)