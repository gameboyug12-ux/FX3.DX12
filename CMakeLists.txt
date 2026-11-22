cmake_minimum_required(VERSION 3.24)
project(FX3DX12 VERSION 0.4.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "FX3.DX12 v0.4 currently targets Windows 10/11 x64.")
endif()

add_executable(FX3DX12 WIN32
  src/main.cpp
  src/ObjLoader.cpp
  src/Texture.cpp
  src/TEX.cpp
  src/Renderer.cpp
)
target_compile_features(FX3DX12 PRIVATE cxx_std_20)
target_compile_definitions(FX3DX12 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(FX3DX12 PRIVATE include)
target_link_libraries(FX3DX12 PRIVATE d3d12 dxgi dxguid d3dcompiler)

if(MSVC)
  target_compile_options(FX3DX12 PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()

add_custom_command(TARGET FX3DX12 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:FX3DX12>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:FX3DX12>/assets
  VERBATIM
)
