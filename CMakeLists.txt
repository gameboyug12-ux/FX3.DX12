cmake_minimum_required(VERSION 3.24)

project(FX3.DX12 VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "FX3.DX12 currently supports Windows only.")
endif()

find_program(DXC_EXECUTABLE NAMES dxc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{WindowsSdkDir}/bin"
    PATH_SUFFIXES x64
)
if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC was not found. Install the DirectX Shader Compiler and make dxc.exe available on PATH.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(FX3_SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
file(MAKE_DIRECTORY "${FX3_SHADER_OUTPUT_DIR}")

set(FX3_SHADER_SOURCES
    shaders/mesh_vs.hlsl
    shaders/mesh_ps.hlsl
)
set(FX3_SHADER_BINARIES)
foreach(SHADER_SOURCE IN LISTS FX3_SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    if(SHADER_NAME STREQUAL "mesh_vs.hlsl")
        set(SHADER_PROFILE vs_6_0)
        set(SHADER_ENTRY VSMain)
    else()
        set(SHADER_PROFILE ps_6_0)
        set(SHADER_ENTRY PSMain)
    endif()
    set(SHADER_BINARY "${FX3_SHADER_OUTPUT_DIR}/${SHADER_NAME}.cso")
    add_custom_command(
        OUTPUT "${SHADER_BINARY}"
        COMMAND "${DXC_EXECUTABLE}" -nologo -E ${SHADER_ENTRY} -T ${SHADER_PROFILE}
                -Ges -WX -Fo "${SHADER_BINARY}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
        COMMENT "Compiling HLSL: ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND FX3_SHADER_BINARIES "${SHADER_BINARY}")
endforeach()
add_custom_target(FX3Shaders DEPENDS ${FX3_SHADER_BINARIES})

add_executable(FX3DX12 WIN32
    src/main.cpp
    src/engine.cpp
)
add_dependencies(FX3DX12 FX3Shaders)

target_include_directories(FX3DX12 PRIVATE include)
target_compile_features(FX3DX12 PRIVATE cxx_std_20)
target_compile_definitions(FX3DX12 PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    FX3_SHADER_DIR=L"${FX3_SHADER_OUTPUT_DIR}"
    $<$<CONFIG:Debug>:FX3_DEBUG>
)

target_link_libraries(FX3DX12 PRIVATE d3d12 dxgi dxguid)

if(MSVC)
    target_compile_options(FX3DX12 PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
endif()

message(STATUS "Configuring FX3.DX12 ${PROJECT_VERSION}")
message(STATUS "Using DXC: ${DXC_EXECUTABLE}")
