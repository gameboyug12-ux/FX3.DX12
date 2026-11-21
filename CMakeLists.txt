cmake_minimum_required(VERSION 3.24)

project(FX3.DX12 VERSION 0.3.5 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "XWe currently supports Windows only.")
endif()

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

add_library(xwe_engine SHARED
    src/engine.cpp
    src/dx12_features.cpp
    src/pso_cache.cpp
    src/dxr_pipeline.cpp
    src/xwe_application.cpp
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

# Java sources are kept visible to IDEs; the native engine does not require a JDK.
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
    foreach(TARGET xwe_engine FX3DX12)
        target_compile_options(${TARGET} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
    endforeach()
endif()

message(STATUS "Configuring FX3.DX12 / XWe ${PROJECT_VERSION}")
message(STATUS "Using DXC: ${DXC_EXECUTABLE}")
