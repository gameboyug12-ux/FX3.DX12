cmake_minimum_required(VERSION 3.24)

project(FX3.DX12 VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "FX3.DX12 currently supports Windows only.")
endif()

add_executable(FX3DX12 WIN32
    src/main.cpp
    src/engine.cpp
)

target_include_directories(FX3DX12 PRIVATE include)
target_compile_features(FX3DX12 PRIVATE cxx_std_20)
target_compile_definitions(FX3DX12 PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    $<$<CONFIG:Debug>:FX3_DEBUG>
)

target_link_libraries(FX3DX12 PRIVATE
    d3d12
    dxgi
    dxguid
)

if(MSVC)
    target_compile_options(FX3DX12 PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
endif()

set_target_properties(FX3DX12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Configuring FX3.DX12 ${PROJECT_VERSION}")
