cmake_minimum_required(VERSION 3.20)
project(TarClient LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Minecraft Bedrock is 64-bit, build with an x64 toolchain")
endif()

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake)
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.4
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake)
# SOURCE_SUBDIR points nowhere so we only download them and build them ourselves below
FetchContent_MakeAvailable(imgui minhook)

# ---- third party (built as static libs) ----
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui PUBLIC d3dcompiler dwmapi)

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

# ---- the client dll ----
file(GLOB_RECURSE TAR_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(TarClient SHARED ${TAR_SOURCES})
set_target_properties(TarClient PROPERTIES PREFIX "")
target_include_directories(TarClient PRIVATE src)
target_compile_definitions(TarClient PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(TarClient PRIVATE imgui minhook d3d11 d3d12 dxgi)
if(MSVC)
    target_compile_options(TarClient PRIVATE /W3 /utf-8)
    set_property(TARGET TarClient imgui minhook PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_link_options(TarClient PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---- tiny injector ----
add_executable(TarInjector injector/Injector.cpp)
target_compile_definitions(TarInjector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(TarInjector PRIVATE advapi32)
if(MSVC)
    set_property(TARGET TarInjector PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_link_options(TarInjector PRIVATE -static -municode)
endif()
