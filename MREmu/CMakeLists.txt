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

# MSVC：
#   1) /wd4828 /wd4819：禁用 SDK 头 vmsys.h（GBK 编码）里的 codepage 非法字节告警。
#   2) /wd4244 /wd4018：兜底屏蔽 int↔float、signed/unsigned 常见转换告警。
#      （Keyboard.cpp 已经用显式 cast 逐一修掉；这里做二次保险，避免将来修改再引入同类告警。）
if(MSVC)
    target_compile_options(MREmu PRIVATE /wd4828 /wd4819 /wd4244 /wd4018)
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