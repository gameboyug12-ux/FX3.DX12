cmake_minimum_required(VERSION 3.24)

project(FX3.DX12 VERSION 0.3.8 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "XWe currently supports Windows only.")
endif()

set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

find_program(DXC_EXECUTABLE NAMES dxc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{WindowsSdkDir}/bin"
    PATH_SUFFIXES x64
)
if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC was not found. Install the DirectX Shader Compiler and make dxc.exe available on PATH.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(XWE_SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
file(MAKE_DIRECTORY "${XWE_SHADER_OUTPUT_DIR}")

set(XWE_SHADER_SOURCES shaders/mesh_vs.hlsl shaders/mesh_ps.hlsl)
set(XWE_SHADER_BINARIES)
foreach(SHADER_SOURCE IN LISTS XWE_SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    if(SHADER_NAME STREQUAL "mesh_vs.hlsl")
        set(SHADER_PROFILE vs_6_0)
        set(SHADER_ENTRY VSMain)
    else()
        set(SHADER_PROFILE ps_6_0)
        set(SHADER_ENTRY PSMain)
    endif()
    set(SHADER_BINARY "${XWE_SHADER_OUTPUT_DIR}/${SHADER_NAME}.cso")
    add_custom_command(
        OUTPUT "${SHADER_BINARY}"
        COMMAND "${DXC_EXECUTABLE}" -nologo -E ${SHADER_ENTRY} -T ${SHADER_PROFILE}
                -Ges -WX -Fo "${SHADER_BINARY}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
        COMMENT "Compiling HLSL: ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND XWE_SHADER_BINARIES "${SHADER_BINARY}")
endforeach()

set(XWE_DXR_SHADER "${XWE_SHADER_OUTPUT_DIR}/pathtrace.dxil")
add_custom_command(
    OUTPUT "${XWE_DXR_SHADER}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -T lib_6_3 -HV 2021 -Ges
            -Fo "${XWE_DXR_SHADER}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pathtrace.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pathtrace.hlsl"
    COMMENT "Compiling DXR library"
    VERBATIM
)
list(APPEND XWE_SHADER_BINARIES "${XWE_DXR_SHADER}")
add_custom_target(XWeShaders DEPENDS ${XWE_SHADER_BINARIES})

# Public API headers are listed for IDE/source-package visibility.
set(XWE_PUBLIC_HEADERS
    include/XWe/Application.h
    include/XWeApplication.h
    include/XWe/Engine.hpp
    include/XWe/RenderFeatures.hpp
    include/XWe/DX12/Dx12Features.hpp
    include/XWe/DX12/DxrPipeline.hpp
    include/XWe/DX12/PipelineStateCache.hpp
    include/XWe/DX/DX12/DX12.hpp
    include/XWe/DX/DX12/DX12AccelerationStructure.hpp
    include/XWe/DX/DX12/DX12Ctx.hpp
    include/XWe/DX/DX12/DX12GBuffer.hpp
    include/XWe/DX/DX12/DX12Materail.hpp
    include/XWe/DX/DX12/DX12PiplineStateObject.hpp
    include/XWe/DX/DX12/DX12RaytracingApi.hpp
    include/XWe/DX/DX12/DX12RaytracingDispatch.hpp
    include/XWe/DX/DX12/DX12RendererApi.hpp
    include/XWe/DX/DX12/DX12RendererStateObject.hpp
    include/XWe/DX/DX12/DX12Shader.hpp
    include/XWe/DX/DX12/DX12ShaderBindingTable.hpp
    include/XWe/DX/DX12/DX12Texture.hpp
    include/XWe/ShadowTracer/DX12/DX12ShadowTrace.hpp
    include/fx3/dx12_features.hpp
    include/fx3/dxr_pipeline.hpp
    include/fx3/engine.hpp
    include/fx3/pso_cache.hpp
    include/fx3/psopt.hpp
    include/fx3/render_features.hpp
)

add_library(xwe_engine SHARED
    src/engine.cpp
    src/dx12_features.cpp
    src/pso_cache.cpp
    src/dxr_pipeline.cpp
    src/xwe_application.cpp
    src/dx12/DX12Ctx.cpp
    src/dx12/DX12PiplineStateObject.cpp
    src/dx12/DX12RendererStateObject.cpp
    src/dx12/DX12RendererApi.cpp
    src/dx12/DX12GBuffer.cpp
    src/dx12/DX12Texture.cpp
    src/dx12/DX12Materail.cpp
    src/dx12/DX12Shader.cpp
    src/dx12/DX12AccelerationStructure.cpp
    src/dx12/DX12ShaderBindingTable.cpp
    src/dx12/DX12RaytracingApi.cpp
    src/dx12/DX12RaytracingDispatch.cpp
    ${XWE_PUBLIC_HEADERS}
)
add_dependencies(xwe_engine XWeShaders)
target_include_directories(xwe_engine PUBLIC include)
target_compile_features(xwe_engine PUBLIC cxx_std_20)
target_compile_definitions(xwe_engine PRIVATE
    XWE_BUILD_DLL
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    XWE_SHADER_DIR=L"${XWE_SHADER_OUTPUT_DIR}"
    $<$<CONFIG:Debug>:XWE_DEBUG>
)
target_link_libraries(xwe_engine PRIVATE d3d12 dxgi dxguid)

add_executable(FX3DX12 WIN32 src/main.cpp)
set_target_properties(FX3DX12 PROPERTIES OUTPUT_NAME "main")
target_include_directories(FX3DX12 PRIVATE include)
target_compile_features(FX3DX12 PRIVATE cxx_std_20)
target_compile_definitions(FX3DX12 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX $<$<CONFIG:Debug>:XWE_DEBUG>)
target_link_libraries(FX3DX12 PRIVATE xwe_engine)

# Standalone Qt editor; separate from the engine executable.
qt_add_executable(FX3Editor MANUAL_FINALIZATION WIN32
    editor/cpp/main.cpp
    editor/cpp/MainWindow.cpp
    editor/cpp/MainWindow.hpp
    editor/cpp/SceneDocument.cpp
    editor/cpp/SceneDocument.hpp
    editor/cpp/DXRViewport.cpp
    editor/cpp/DXRViewport.hpp
)
add_dependencies(FX3Editor XWeShaders)
target_compile_features(FX3Editor PRIVATE cxx_std_20)
target_compile_definitions(FX3Editor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(FX3Editor PRIVATE "XWE_SHADER_DIR=L\""${XWE_SHADER_OUTPUT_DIR}\""")
target_link_libraries(FX3Editor PRIVATE Qt6::Widgets d3d12 dxgi dxguid)
qt_finalize_executable(FX3Editor)

set(XWE_JAVA_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xwe/java/api/XWeApplication.java
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xwe/java/api/XWeApplicationDesc.java
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xwe/java/api/XWeException.java
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xwe/java/api/XWeNative.java
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xwe/java/api/XWeVersion.java
)
add_custom_target(XWeJavaApi SOURCES ${XWE_JAVA_API_SOURCES})
find_package(Java 22 COMPONENTS Development QUIET)
if(Java_FOUND)
    add_custom_target(XWeJavaCompile
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/java"
        COMMAND "${Java_JAVAC_EXECUTABLE}" --release 22
                -d "${CMAKE_BINARY_DIR}/java" ${XWE_JAVA_API_SOURCES}
        DEPENDS ${XWE_JAVA_API_SOURCES}
        COMMENT "Compiling XWe Java 22 Panama API"
        VERBATIM
    )
endif()

if(MSVC)
    foreach(TARGET xwe_engine FX3DX12 FX3Editor)
        target_compile_options(${TARGET} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
    endforeach()
endif()

message(STATUS "Configuring FX3.DX12 / XWe ${PROJECT_VERSION}")
message(STATUS "Using DXC: ${DXC_EXECUTABLE}")
