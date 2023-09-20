cmake_minimum_required (VERSION 3.6)

project(Sapphire CXX)

set(SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.cpp
)

set(INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.hpp
)


add_subdirectory(3rdparty/DiligentEngine/DiligentCore)
add_subdirectory(3rdparty/DiligentEngine/DiligentFX)
add_subdirectory(3rdparty/DiligentEngine/DiligentTools)
add_subdirectory(3rdparty/DiligentEngine/DiligentSamples/SampleBase)

add_target_platform_app(${PROJECT_NAME} "${SOURCE}" "${INCLUDE}" "")
target_compile_options(${PROJECT_NAME} PRIVATE -DUNICODE -DENGINE_DLL)

# get supported rendering backends - on Widows, that will be OpenGL, Vulcan, DX12
get_supported_backends(ENGINE_LIBRARIES)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    
    Diligent-BuildSettings
PUBLIC
    Diligent-SampleBase
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-TextureLoader
    Diligent-TargetPlatform
    Diligent-Imgui
    Diligent-AssetLoader
    Diligent-GraphicsAccessories
    ${ENGINE_LIBRARIES}
    Diligent-NativeAppBase
    DiligentFX
)
copy_required_dlls(${PROJECT_NAME})