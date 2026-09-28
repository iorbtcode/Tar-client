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

# opengl backend, only the java dll uses it
add_library(imgui_gl2 STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp)
target_link_libraries(imgui_gl2 PUBLIC imgui opengl32)

# ---- the client dlls ----
# everything under src/ is shared, except the renderer hook which is picked per edition
file(GLOB_RECURSE TAR_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER TAR_SOURCES EXCLUDE REGEX "src/hooks/(dx|gl)/")

function(tar_client name)
    add_library(${name} SHARED ${TAR_SOURCES} ${ARGN})
    set_target_properties(${name} PROPERTIES PREFIX "")
    target_include_directories(${name} PRIVATE src)
    target_compile_definitions(${name} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(${name} PRIVATE imgui minhook)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W3 /utf-8)
        set_property(TARGET ${name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_link_options(${name} PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endfunction()

# bedrock: hooks the dx12 / dx11 swap chain
file(GLOB TAR_DX_SOURCES CONFIGURE_DEPENDS src/hooks/dx/*.cpp)
tar_client(TarClient ${TAR_DX_SOURCES})
target_link_libraries(TarClient PRIVATE d3d11 d3d12 dxgi)

# java: hooks wglSwapBuffers
file(GLOB TAR_GL_SOURCES CONFIGURE_DEPENDS src/hooks/gl/*.cpp)
tar_client(TarClientJava ${TAR_GL_SOURCES})
target_compile_definitions(TarClientJava PRIVATE TAR_JAVA)
target_link_libraries(TarClientJava PRIVATE imgui_gl2 opengl32)

if(MSVC)
    set_property(TARGET imgui imgui_gl2 minhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
