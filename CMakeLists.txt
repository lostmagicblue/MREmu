cmake_minimum_required (VERSION 3.8)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MREmu")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# MSVC 统一静态 CRT（/MT Release / /MTd Debug），消灭 LNK4098 LIBCMT / MSVCRT 互相冲突
# SFML/spdlog/unicorn 都是从源码构建，统一之后 CRT 选择一致就不会告警
if(MSVC)
  foreach(flag_var
      CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
    if(${flag_var} MATCHES "/MDd")
      string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()
  # 保证 C++ 异常模型一致，避免 deps 之间默认值漂移
  add_compile_options(/EHsc)
  # 压掉第三方 SDK/deps 的海量无害警告：C4099(class≠struct PDB名)
  add_compile_options(/wd4099)
endif()

# 第三方 SDK/deps 里大量 strcpy/sprintf/sscanf/localtime，全局关掉 C4996 不安全 CRT 告警
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

# Enable optimizations by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

option(CAPSTONE "Endble capstone" OFF)

if(CAPSTONE)
    add_definitions(-DCAPSTONE)
endif()

add_definitions(-DLIBICONV_PLUG)
set(BUILD_SHARED_LIBS FALSE) 

# 强制指定 MRE SDK 路径为当前目录下的 mre_sdk 文件夹
set(MRE_SDK_PATH "${CMAKE_SOURCE_DIR}/mre_sdk" CACHE PATH "Path to MRE SDK" FORCE)
message(STATUS "MRE SDK path: ${MRE_SDK_PATH}")

include_directories("include")

add_subdirectory (deps)

add_subdirectory (MREmu)

# Copy fonts/ directory next to executable at build time (optional CJK TTF fallback)
if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/fonts")
    add_custom_target(copy_fonts ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/fonts"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts"
        COMMENT "Copying fonts/ to bin/fonts/"
    )
endif()
