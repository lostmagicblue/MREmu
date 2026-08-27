cmake_minimum_required (VERSION 3.8)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MREmu")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

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

# MRE SDK path: can be set via -DMRE_SDK_PATH=... or $ENV{MRE_SDK}
if(DEFINED ENV{MRE_SDK})
    set(MRE_SDK_PATH "$ENV{MRE_SDK}" CACHE PATH "Path to MRE SDK")
elseif(NOT DEFINED MRE_SDK_PATH)
    set(MRE_SDK_PATH "${CMAKE_SOURCE_DIR}/sdk" CACHE PATH "Path to MRE SDK")
endif()
message(STATUS "MRE SDK path: ${MRE_SDK_PATH}")

include_directories("include")

add_subdirectory (deps)

add_subdirectory (MREmu)
