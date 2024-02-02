cmake_minimum_required (VERSION 3.6)

project(Sapphire CXX)

set(SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GrimrockModelLoader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp
    ${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp
)

set(INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.hpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GrimrockModelLoader.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
)

set(ASSETS
    assets/DGLogo.png
)

set(ALL_ASSETS ${ASSETS} ${SHADERS})

source_group("assets" FILES ${ALL_ASSETS})  


add_subdirectory(3rdparty/DiligentEngine/DiligentCore)
add_subdirectory(3rdparty/DiligentEngine/DiligentFX)
add_subdirectory(3rdparty/DiligentEngine/DiligentTools)
add_subdirectory(src/AppBase)

add_target_platform_app(${PROJECT_NAME} "${SOURCE}" "${INCLUDE}" "${ALL_ASSETS}")
target_compile_options(${PROJECT_NAME} PRIVATE -DUNICODE -DENGINE_DLL)
#enable c++ 17 (clipper2 support)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE 3rdparty/gainput/include)
target_include_directories(${PROJECT_NAME} PRIVATE 3rdparty/Clipper2/CPP/Clipper2Lib/include)
target_include_directories(${PROJECT_NAME} PRIVATE 3rdparty/earcut/include)

target_include_directories(${PROJECT_NAME} PRIVATE 3rdparty/lua)

# get supported rendering backends - on Widows, that will be OpenGL, Vulcan, DX12
get_supported_backends(ENGINE_LIBRARIES)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    
    Diligent-BuildSettings
PUBLIC
    AppBase
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

set(LUA_LIB_DIR ${CMAKE_SOURCE_DIR}/3rdparty/lua)

find_library(LUA_LIB lua51 "${LUA_LIB_DIR}")
message(${LUA_LIB})

target_link_libraries(${PROJECT_NAME}
PRIVATE
${LUA_LIB}
)

if(PLATFORM_WIN32 OR PLATFORM_LINUX)
# set debugger working folder
    set_target_properties(${PROJECT_NAME} PROPERTIES 
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        )
    # Copy assets to target folder
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "\"$<TARGET_FILE_DIR:${PROJECT_NAME}>\"")

    # Add a post-build command to copy the DLL file
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${LUA_LIB_DIR}/lua51.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()
copy_required_dlls(${PROJECT_NAME})

