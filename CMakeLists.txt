cmake_minimum_required(VERSION 3.10)

project(gameserver VERSION 0.0.01 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Source files for gameserver executable
set(GAMESERVER_SOURCES
	src/main.cpp
	# Services
	src/services/navigation_service.cpp
	src/services/astar_pathfinding.cpp
	src/services/visualizer_service.cpp
	src/services/network_service.cpp
	# Systems - Core
	src/systems/gameserver.cpp
	src/systems/gameplay_coordinator.cpp
	# Systems - Core Game Logic
	src/systems/core/collision_system.cpp
	src/systems/core/combat_system.cpp
	src/systems/core/brain_system.cpp
	src/systems/core/input_system.cpp
	src/systems/core/movement_system.cpp
	src/systems/core/network_sync_system.cpp
	src/systems/core/npc_system.cpp
	src/systems/core/spawning_system.cpp
	src/systems/core/wave_system.cpp
	# Systems - Utilities
	src/systems/util/combat_calculator.cpp
	src/systems/util/data_loader.cpp
	src/systems/util/packet_handler.cpp
	src/systems/util/player_manager.cpp
	src/systems/util/serialization_system.cpp
	src/systems/util/targeting_utility.cpp
)

# Header files are now included via consolidated headers:
# - <components/component.hpp> for all component types
# - <systems/system.hpp> for all system types
# - <services/service.hpp> for all service types
# No individual header listing needed.

set(LIBRARY_SOURCES
	src/libs/pugixml.cpp
)

add_executable(gameserver ${GAMESERVER_SOURCES} ${LIBRARY_SOURCES})

# Include directories
target_include_directories(gameserver PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/libs
	${CMAKE_SOURCE_DIR}/src/components
	${CMAKE_SOURCE_DIR}/src/systems
	${CMAKE_SOURCE_DIR}/src/entities
)

# Set warning levels
if (MSVC)
	target_compile_options(gameserver PRIVATE /W4)
else()
	target_compile_options(gameserver PRIVATE -Wall -Wextra -pedantic)
endif()

# Enable testing
enable_testing()

# Add test executable
add_executable(gameserver_tests
	tests/tests.cpp
	src/systems/util/data_loader.cpp
	src/libs/pugixml.cpp
)

target_include_directories(gameserver_tests PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/libs
	${CMAKE_SOURCE_DIR}/src/components
	${CMAKE_SOURCE_DIR}/src/systems
	${CMAKE_SOURCE_DIR}/src/entities
)

if (MSVC)
	target_compile_options(gameserver_tests PRIVATE /W4)
else()
	target_compile_options(gameserver_tests PRIVATE -Wall -Wextra -pedantic)
endif()

# Set test output directory
set_target_properties(gameserver_tests PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# Register the test
add_test(NAME GameServerTests COMMAND gameserver_tests)
