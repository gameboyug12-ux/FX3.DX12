cmake_minimum_required(VERSION 3.24)
project(FX3.DX12 VERSION 0.3.9 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "FX3.DX12 currently supports Windows only.")
endif()

option(FX3_BUILD_ENGINE_STATIC "Build static engine (.lib)" ON)
option(FX3_BUILD_ENGINE_SHARED "Build dynamic engine (.dll)" ON)
option(FX3_BUILD_RUNTIME "Build main.exe" ON)
option(FX3_BUILD_EDITOR "Build the native Win32 FX3 editor" ON)
option(FX3_ENABLE_FSR3 "Enable AMD FSR 3 runtime discovery" ON)
set(FX3_FSR3_DLL_DIR "" CACHE PATH "Directory containing AMD FSR 3 x64 runtime DLLs")

set(FX3_FSR3_RUNTIME_DLLS
    ffx_fsr3_x64.dll
    ffx_fsr3upscaler_x64.dll
    ffx_opticalflow_x64.dll
    ffx_frameinterpolation_x64.dll
    ffx_backend_dx12_x64.dll)

if(FX3_ENABLE_FSR3 AND FX3_FSR3_DLL_DIR)
    foreach(dll IN LISTS FX3_FSR3_RUNTIME_DLLS)
        if(NOT EXISTS "${FX3_FSR3_DLL_DIR}/${dll}")
            message(FATAL_ERROR "Missing FSR3 runtime DLL: ${FX3_FSR3_DLL_DIR}/${dll}")
        endif()
    endforeach()
endif()

function(fx3_deploy_fsr3 target)
    if(FX3_ENABLE_FSR3 AND FX3_FSR3_DLL_DIR)
        set(destination "$<TARGET_FILE_DIR:${target}>/third_party/FSR")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
            VERBATIM)
        foreach(dll IN LISTS FX3_FSR3_RUNTIME_DLLS)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${FX3_FSR3_DLL_DIR}/${dll}" "${destination}/${dll}"
                VERBATIM)
        endforeach()
    endif()
endfunction()
set(FX3_LINK_ENGINE "DYNAMIC" CACHE STRING "Engine linked by EXE targets: DYNAMIC or STATIC")
set_property(CACHE FX3_LINK_ENGINE PROPERTY STRINGS DYNAMIC STATIC)
string(TOUPPER "${FX3_LINK_ENGINE}" FX3_LINK_ENGINE)
if(NOT FX3_LINK_ENGINE MATCHES "^(DYNAMIC|STATIC)$")
    message(FATAL_ERROR "FX3_LINK_ENGINE must be DYNAMIC or STATIC.")
endif()
if(FX3_LINK_ENGINE STREQUAL "DYNAMIC" AND NOT FX3_BUILD_ENGINE_SHARED)
    message(FATAL_ERROR "DYNAMIC linking requires FX3_BUILD_ENGINE_SHARED=ON.")
endif()
if(FX3_LINK_ENGINE STREQUAL "STATIC" AND NOT FX3_BUILD_ENGINE_STATIC)
    message(FATAL_ERROR "STATIC linking requires FX3_BUILD_ENGINE_STATIC=ON.")
endif()
if(NOT FX3_BUILD_ENGINE_STATIC AND NOT FX3_BUILD_ENGINE_SHARED)
    message(FATAL_ERROR "At least one engine variant must be enabled.")
endif()

set(FX3_OUT_DIR "C:/XWe0.2/out" CACHE PATH "FX3 output root")
set(FX3_DYNAMIC_DIR "${FX3_OUT_DIR}/dynamic")
set(FX3_STATIC_DIR "${FX3_OUT_DIR}/static")
set(FX3_EDITORS_DIR "${FX3_OUT_DIR}/editors")
set(FX3_SHADER_DIR "${FX3_OUT_DIR}/shaders")
file(MAKE_DIRECTORY "${FX3_OUT_DIR}" "${FX3_DYNAMIC_DIR}" "${FX3_STATIC_DIR}" "${FX3_EDITORS_DIR}" "${FX3_SHADER_DIR}")

find_program(DXC_EXECUTABLE NAMES dxc HINTS "C:/XWe0.2/cmp-utils/dx" "$ENV{VULKAN_SDK}/Bin" "$ENV{WindowsSdkDir}/bin" PATH_SUFFIXES x64)
if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC not found. Set -DDXC_EXECUTABLE=<path-to-dxc.exe>.")
endif()

function(fx3_output_dirs target dir)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${dir}" LIBRARY_OUTPUT_DIRECTORY "${dir}" ARCHIVE_OUTPUT_DIRECTORY "${dir}")
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} "${dir}" LIBRARY_OUTPUT_DIRECTORY_${cfg} "${dir}" ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${dir}")
    endforeach()
endfunction()

set(XWE_SHADER_BINARIES)
foreach(shader IN ITEMS
    shaders/mesh_vs.hlsl shaders/mesh_ps.hlsl
    shaders/upscale_vs.hlsl shaders/upscale_ps.hlsl)
    get_filename_component(shader_name "${shader}" NAME)
    if(shader_name MATCHES "_vs\\.hlsl$")
        set(shader_profile vs_6_0)
        set(shader_entry VSMain)
    else()
        set(shader_profile ps_6_0)
        set(shader_entry PSMain)
    endif()
    set(shader_output "${FX3_SHADER_DIR}/${shader_name}.cso")
    add_custom_command(OUTPUT "${shader_output}"
        COMMAND "${DXC_EXECUTABLE}" -nologo -E "${shader_entry}" -T "${shader_profile}" -Ges -WX -Fo "${shader_output}" "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" COMMENT "Compiling HLSL: ${shader_name}" VERBATIM)
    list(APPEND XWE_SHADER_BINARIES "${shader_output}")
endforeach()

set(XWE_DXR_SHADER "${FX3_SHADER_DIR}/pathtrace.dxil")
add_custom_command(OUTPUT "${XWE_DXR_SHADER}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -T lib_6_3 -HV 2021 -Ges -Fo "${XWE_DXR_SHADER}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pathtrace.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pathtrace.hlsl" COMMENT "Compiling legacy DXR preview" VERBATIM)
list(APPEND XWE_SHADER_BINARIES "${XWE_DXR_SHADER}")

set(XWE_HYBRID_DXR_SHADER "${FX3_SHADER_DIR}/hybrid_pathtrace.dxil")
add_custom_command(OUTPUT "${XWE_HYBRID_DXR_SHADER}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -T lib_6_3 -HV 2021 -Ges -Fo "${XWE_HYBRID_DXR_SHADER}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hybrid_pathtrace.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hybrid_pathtrace.hlsl" COMMENT "Compiling hybrid DXR path tracer" VERBATIM)
list(APPEND XWE_SHADER_BINARIES "${XWE_HYBRID_DXR_SHADER}")

set(XWE_HYBRID_DEFERRED_SHADER "${FX3_SHADER_DIR}/hybrid_deferred.dxil")
add_custom_command(OUTPUT "${XWE_HYBRID_DEFERRED_SHADER}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -T lib_6_3 -HV 2021 -Ges
            -Fo "${XWE_HYBRID_DEFERRED_SHADER}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hybrid_deferred.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hybrid_deferred.hlsl"
    COMMENT "Compiling raster-primary hybrid DXR library" VERBATIM)
list(APPEND XWE_SHADER_BINARIES "${XWE_HYBRID_DEFERRED_SHADER}")
add_custom_target(XWeShaders DEPENDS ${XWE_SHADER_BINARIES})

set(XWE_PUBLIC_HEADERS
    include/XWe/Application.h include/XWeApplication.h include/XWe/Engine.hpp include/XWe/RenderFeatures.hpp
    include/XWe/DX12/Dx12Features.hpp include/XWe/DX12/DxrPipeline.hpp include/XWe/DX12/PipelineStateCache.hpp
    include/XWe/DX/DX12/DX12.hpp include/XWe/DX/DX12/DX12AccelerationStructure.hpp include/XWe/DX/DX12/DX12Ctx.hpp
    include/XWe/DX/DX12/DX12GBuffer.hpp include/XWe/DX/DX12/DX12Materail.hpp include/XWe/DX/DX12/DX12PiplineStateObject.hpp
    include/XWe/DX/DX12/DX12RaytracingApi.hpp include/XWe/DX/DX12/DX12RaytracingDispatch.hpp include/XWe/DX/DX12/DX12RaytracingSceneData.hpp
    include/XWe/DX/DX12/DX12RendererApi.hpp include/XWe/DX/DX12/DX12RendererStateObject.hpp include/XWe/DX/DX12/DX12Shader.hpp
    include/XWe/DX/DX12/DX12ShaderBindingTable.hpp include/XWe/DX/DX12/DX12Texture.hpp
    include/XWe/ShadowTracer/DX12/DX12ShadowTrace.hpp
    include/XWe/Rendering/Upscaling/FSR3Loader.hpp
    include/XWe/Rendering/Upscaling/UpscalingSettings.hpp
    include/fx3/dx12_features.hpp include/fx3/dxr_pipeline.hpp
    include/fx3/engine.hpp include/fx3/pso_cache.hpp include/fx3/psopt.hpp include/fx3/render_features.hpp)

set(XWE_ENGINE_SOURCES
    src/engine.cpp src/dx12_features.cpp src/pso_cache.cpp src/dxr_pipeline.cpp src/xwe_application.cpp
    src/dx12/DX12Ctx.cpp src/dx12/DX12PiplineStateObject.cpp src/dx12/DX12RendererStateObject.cpp
    src/dx12/DX12RendererApi.cpp src/dx12/DX12GBuffer.cpp src/dx12/DX12Texture.cpp src/dx12/DX12Materail.cpp
    src/dx12/DX12Shader.cpp src/dx12/DX12AccelerationStructure.cpp src/dx12/DX12ShaderBindingTable.cpp
    src/dx12/DX12RaytracingApi.cpp src/dx12/DX12RaytracingDispatch.cpp
    src/rendering/upscaling/FSR3Loader.cpp ${XWE_PUBLIC_HEADERS})

if(FX3_BUILD_ENGINE_SHARED)
    add_library(xwe_engine_shared SHARED ${XWE_ENGINE_SOURCES})
    add_dependencies(xwe_engine_shared XWeShaders)
    target_include_directories(xwe_engine_shared PUBLIC include)
    target_compile_features(xwe_engine_shared PUBLIC cxx_std_20)
    target_compile_definitions(xwe_engine_shared PRIVATE XWE_BUILD_DLL UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX "XWE_SHADER_DIR=L\"${FX3_SHADER_DIR}\"" $<$<CONFIG:Debug>:XWE_DEBUG> $<$<BOOL:${FX3_ENABLE_FSR3}>:FX3_ENABLE_FSR3=1>)
    target_link_libraries(xwe_engine_shared PUBLIC d3d12 dxgi dxguid)
    set_target_properties(xwe_engine_shared PROPERTIES OUTPUT_NAME xwe_engine WINDOWS_EXPORT_ALL_SYMBOLS ON)
    fx3_output_dirs(xwe_engine_shared "${FX3_DYNAMIC_DIR}")
endif()

if(FX3_BUILD_ENGINE_STATIC)
    add_library(xwe_engine_static STATIC ${XWE_ENGINE_SOURCES})
    add_dependencies(xwe_engine_static XWeShaders)
    target_include_directories(xwe_engine_static PUBLIC include)
    target_compile_features(xwe_engine_static PUBLIC cxx_std_20)
    target_compile_definitions(xwe_engine_static PUBLIC XWE_STATIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX "XWE_SHADER_DIR=L\"${FX3_SHADER_DIR}\"" $<$<CONFIG:Debug>:XWE_DEBUG> $<$<BOOL:${FX3_ENABLE_FSR3}>:FX3_ENABLE_FSR3=1>)
    target_link_libraries(xwe_engine_static PUBLIC d3d12 dxgi dxguid)
    set_target_properties(xwe_engine_static PROPERTIES OUTPUT_NAME xwe_engine)
    fx3_output_dirs(xwe_engine_static "${FX3_STATIC_DIR}")
endif()

if(FX3_LINK_ENGINE STREQUAL "DYNAMIC")
    set(FX3_ENGINE_TARGET xwe_engine_shared)
else()
    set(FX3_ENGINE_TARGET xwe_engine_static)
endif()

if(FX3_BUILD_RUNTIME)
    add_executable(FX3DX12 WIN32 src/main.cpp)
    set_target_properties(FX3DX12 PROPERTIES OUTPUT_NAME main)
    target_include_directories(FX3DX12 PRIVATE include)
    target_compile_features(FX3DX12 PRIVATE cxx_std_20)
    target_compile_definitions(FX3DX12 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX $<$<CONFIG:Debug>:XWE_DEBUG>)
    target_link_libraries(FX3DX12 PRIVATE ${FX3_ENGINE_TARGET})
    add_dependencies(FX3DX12 XWeShaders)
    fx3_output_dirs(FX3DX12 "${FX3_OUT_DIR}")
    fx3_deploy_fsr3(FX3DX12)
    if(FX3_LINK_ENGINE STREQUAL "DYNAMIC")
        add_custom_command(TARGET FX3DX12 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:xwe_engine_shared>" "$<TARGET_FILE_DIR:FX3DX12>" VERBATIM)
    endif()
endif()

if(FX3_BUILD_EDITOR)
    set(FX3_EDITOR_SOURCES editor/win32/main.cpp editor/win32/MainWindow.cpp editor/win32/MainWindow.hpp editor/win32/SceneDocument.cpp editor/win32/SceneDocument.hpp editor/win32/DXRViewport.cpp editor/win32/DXRViewport.hpp)
    foreach(source IN LISTS FX3_EDITOR_SOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            message(FATAL_ERROR "Missing native editor source: ${source}")
        endif()
    endforeach()
    add_executable(FX3Editor WIN32 ${FX3_EDITOR_SOURCES})
    add_dependencies(FX3Editor XWeShaders)
    target_include_directories(FX3Editor PRIVATE editor/win32 include)
    target_compile_features(FX3Editor PRIVATE cxx_std_20)
    target_compile_definitions(FX3Editor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX "XWE_SHADER_DIR=L\"${FX3_SHADER_DIR}\"" $<$<CONFIG:Debug>:XWE_DEBUG>)
    target_link_libraries(FX3Editor PRIVATE ${FX3_ENGINE_TARGET} d3d12 dxgi dxguid ole32 shell32 comctl32 uuid)
    fx3_output_dirs(FX3Editor "${FX3_EDITORS_DIR}")
    fx3_deploy_fsr3(FX3Editor)
    if(FX3_LINK_ENGINE STREQUAL "DYNAMIC")
        add_custom_command(TARGET FX3Editor POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:xwe_engine_shared>" "$<TARGET_FILE_DIR:FX3Editor>" VERBATIM)
    endif()
endif()

set(XWE_JAVA_API_SOURCES src/xwe/java/api/XWeApplication.java src/xwe/java/api/XWeApplicationDesc.java src/xwe/java/api/XWeException.java src/xwe/java/api/XWeNative.java src/xwe/java/api/XWeVersion.java)
add_custom_target(XWeJavaApi SOURCES ${XWE_JAVA_API_SOURCES})
find_package(Java 22 COMPONENTS Development QUIET)
if(Java_FOUND)
    add_custom_target(XWeJavaCompile COMMAND "${CMAKE_COMMAND}" -E make_directory "${FX3_OUT_DIR}/java" COMMAND "${Java_JAVAC_EXECUTABLE}" --release 22 -d "${FX3_OUT_DIR}/java" ${XWE_JAVA_API_SOURCES} DEPENDS ${XWE_JAVA_API_SOURCES} COMMENT "Compiling XWe Java 22 Panama API" VERBATIM)
endif()

if(MSVC)
    foreach(target IN ITEMS xwe_engine_shared xwe_engine_static FX3DX12 FX3Editor)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
        endif()
    endforeach()
endif()

message(STATUS "FX3 output root: ${FX3_OUT_DIR}")
message(STATUS "Engine variants: static=${FX3_BUILD_ENGINE_STATIC}, shared=${FX3_BUILD_ENGINE_SHARED}")
message(STATUS "EXE engine linkage: ${FX3_LINK_ENGINE}")
message(STATUS "DXC: ${DXC_EXECUTABLE}")
message(STATUS "FSR3 runtime discovery: ${FX3_ENABLE_FSR3}")
message(STATUS "FSR3 DLL directory: ${FX3_FSR3_DLL_DIR}")
