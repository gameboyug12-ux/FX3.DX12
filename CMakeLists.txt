cmake_minimum_required(VERSION 3.24)

project(FX3DX12
    VERSION 0.4.0
    DESCRIPTION "FX3.DX12 renderer and XWe engine"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "FX3.DX12 and XWe currently require Windows 10/11.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit Windows builds are supported.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FX3_BUILD_RENDERER "Build the standalone FX3 renderer executable" ON)
option(XWE_BUILD_ENGINE "Build the XWe engine shared library" ON)
option(XWE_ENABLE_DEBUG_LAYER "Enable the D3D12 debug layer in Debug builds" ON)
option(XWE_ENABLE_FSR3 "Enable optional FSR3 runtime loader support" OFF)

set(FX3_COMMON_DEFINITIONS UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# ---------------------------------------------------------------------------
# FX3 renderer executable (the v0.4 OBJ + FXTEX sample renderer)
# ---------------------------------------------------------------------------
if(FX3_BUILD_RENDERER)
    add_executable(FX3DX12 WIN32
        src/main.cpp
        src/ObjLoader.cpp
        src/Texture.cpp
        src/TEX.cpp
        src/Renderer.cpp
    )

    target_include_directories(FX3DX12 PRIVATE include)
    target_compile_definitions(FX3DX12 PRIVATE ${FX3_COMMON_DEFINITIONS})
    target_compile_features(FX3DX12 PRIVATE cxx_std_20)
    target_link_libraries(FX3DX12 PRIVATE d3d12 dxgi dxguid d3dcompiler)

    if(MSVC)
        target_compile_options(FX3DX12 PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()

    add_custom_command(TARGET FX3DX12 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                "$<TARGET_FILE_DIR:FX3DX12>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:FX3DX12>/assets"
        VERBATIM
    )
endif()

# ---------------------------------------------------------------------------
# XWe engine DLL (public C ABI + internal Direct3D 12 implementation)
# ---------------------------------------------------------------------------
if(XWE_BUILD_ENGINE)
    set(XWE_SOURCES
        src/xwe_application.cpp
        src/engine.cpp
        src/dx12_features.cpp
        src/dxr_pipeline.cpp
        src/pso_cache.cpp
        src/dx12/DX12AccelerationStructure.cpp
        src/dx12/DX12Ctx.cpp
        src/dx12/DX12GBuffer.cpp
        src/dx12/DX12Materail.cpp
        src/dx12/DX12PiplineStateObject.cpp
        src/dx12/DX12RaytracingApi.cpp
        src/dx12/DX12RaytracingDispatch.cpp
        src/dx12/DX12RendererApi.cpp
        src/dx12/DX12RendererStateObject.cpp
        src/dx12/DX12Shader.cpp
        src/dx12/DX12ShaderBindingTable.cpp
        src/dx12/DX12Texture.cpp
        src/rendering/upscaling/FSR3Loader.cpp
        src/rendering/upscaling/UpscalingSettings.cpp
    )

    add_library(xwe_engine SHARED ${XWE_SOURCES})

    target_include_directories(xwe_engine
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    target_compile_definitions(xwe_engine
        PRIVATE
            ${FX3_COMMON_DEFINITIONS}
            XWE_BUILD_DLL
            XWE_SHADER_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            $<$<CONFIG:Debug>:XWE_DEBUG>
            $<$<BOOL:${XWE_ENABLE_FSR3}>:FX3_ENABLE_FSR3>
    )

    target_compile_features(xwe_engine PUBLIC cxx_std_20)
    target_link_libraries(xwe_engine PRIVATE
        d3d12
        dxgi
        dxguid
        d3dcompiler
        dxcompiler
    )

    if(MSVC)
        target_compile_options(xwe_engine PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()

    if(XWE_ENABLE_DEBUG_LAYER)
        target_compile_definitions(xwe_engine PRIVATE $<$<CONFIG:Debug>:XWE_DEBUG>)
    endif()

    # Keep runtime shaders beside the produced DLL.
    add_custom_command(TARGET xwe_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                "$<TARGET_FILE_DIR:xwe_engine>/shaders"
        VERBATIM
    )
endif()

# Stable output folders for all common Visual Studio configurations.
foreach(_target IN ITEMS FX3DX12 xwe_engine)
    if(TARGET ${_target})
        set_target_properties(${_target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        )
        foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
            string(TOUPPER "${_config}" _config_upper)
            set_target_properties(${_target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/bin/${_config}"
                LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/bin/${_config}"
                ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/lib/${_config}"
            )
        endforeach()
    endif()
endforeach()
