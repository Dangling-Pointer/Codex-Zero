cmake_minimum_required(VERSION 3.24)
project(CodexZero LANGUAGES C CXX)

include(CTest)
add_subdirectory(thirdparty/box2d EXCLUDE_FROM_ALL)
add_subdirectory(thirdparty/enet EXCLUDE_FROM_ALL  )

# ENet 1.3.18 links these libraries for MinGW itself. Add the equivalent
# transitive dependencies for the bundled MSVC build.
if(MSVC)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()

option(ELYSIA_ENABLE_ASAN "Enable AddressSanitizer for project targets" OFF)
option(ELYSIA_ENABLE_IMGUI "Build the optional Dear ImGui development overlay" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(cmake/SDLDependencies.cmake)

if(ELYSIA_ENABLE_IMGUI)
    add_subdirectory(thirdparty/imgui)
endif()

add_subdirectory(engine)
add_subdirectory(game)

option(ELYSIA_BUILD_PHYSICS_BENCHMARK "Build the standalone physics timing probe" OFF)
if(ELYSIA_BUILD_PHYSICS_BENCHMARK)
    add_executable(physics_benchmark tools/benchmarks/physics_benchmark.cpp)
    target_link_libraries(physics_benchmark PRIVATE engine_lib)
endif()

if(ELYSIA_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
    check_cxx_source_compiles("int main() { return 0; }" ELYSIA_ASAN_AVAILABLE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT ELYSIA_ASAN_AVAILABLE)
        message(FATAL_ERROR "ELYSIA_ENABLE_ASAN=ON, but this compiler has no usable AddressSanitizer runtime.")
    endif()
    foreach(ELYSIA_ASAN_TARGET engine_lib game_lib)
        target_compile_options(${ELYSIA_ASAN_TARGET} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${ELYSIA_ASAN_TARGET} INTERFACE -fsanitize=address)
    endforeach()
endif()


add_executable(CodexZero "main.cpp")

target_link_libraries(CodexZero PRIVATE game_lib)

elysia_copy_sdl_runtime(CodexZero)
