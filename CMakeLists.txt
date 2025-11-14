cmake_minimum_required(VERSION 3.10)

project(openchamp_gameserver)

# Detect platform and architecture
message(STATUS "CMake system name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMake system processor: ${CMAKE_SYSTEM_PROCESSOR}")

# Optionally allow user to specify target architecture
set(TARGET_ARCH "x64" CACHE STRING "Target architecture (x64 or arm)")

# Set output directories for different platforms/architectures
if (WIN32)
	set(PLATFORM "windows")
elseif (APPLE)
	set(PLATFORM "macos")
elseif (UNIX)
	set(PLATFORM "linux")
else()
	set(PLATFORM "unknown")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/_output/${PLATFORM}_${TARGET_ARCH})

# Architecture-specific flags
if (TARGET_ARCH STREQUAL "arm")
	if (WIN32)
		# Requires MSVC ARM toolchain
		set(CMAKE_GENERATOR_PLATFORM ARM)
	elseif (APPLE)
		# Apple Silicon Macs
		set(CMAKE_OSX_ARCHITECTURES "arm64")
	elseif (UNIX)
		# Linux ARM (cross-compile)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
	endif()
else()
	if (APPLE)
		set(CMAKE_OSX_ARCHITECTURES "x86_64")
	endif()
endif()

add_executable(gameserver src/main.cpp)