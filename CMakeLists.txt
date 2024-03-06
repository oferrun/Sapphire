cmake_minimum_required (VERSION 3.6)

project(Sapphire CXX)

set(CORE_SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/src/core/allocator.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/camera.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/config.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/error.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/json.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/os.win32.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/sprintf.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/task_system.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/temp_allocator.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/unicode.c
)

set(CORE_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/src/core/allocator.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/array.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/camera.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/config.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/error.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/hash.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/json.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/murmurhash64a.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/os.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/sapphire_macros.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/sapphire_math.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/sapphire_types.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/sprintf.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/task_system.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/temp_allocator.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/unicode.h
)

set(APP_SOURCE
${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.cpp
${CMAKE_CURRENT_LIST_DIR}/src/ImGuizmo/ImGuizmo.cpp
${CMAKE_CURRENT_LIST_DIR}/src/font_system.c
${CMAKE_CURRENT_LIST_DIR}/src/scene.c
${CMAKE_CURRENT_LIST_DIR}/src/renderer.c
${CMAKE_CURRENT_LIST_DIR}/src/grimrock.c
${CMAKE_CURRENT_LIST_DIR}/src/testc.c
${CMAKE_CURRENT_LIST_DIR}/src/imgui/cimgui.cpp
${CMAKE_CURRENT_LIST_DIR}/src/imgui/cimguizmo.cpp
${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp
${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp
${CMAKE_CURRENT_LIST_DIR}/3rdparty/Clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp
)

set(SOURCE ${APP_SOURCE} ${CORE_SOURCE})

set(APP_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}/src/SapphireApp.hpp    
    ${CMAKE_CURRENT_LIST_DIR}/src/sapphire_renderer.h
    ${CMAKE_CURRENT_LIST_DIR}/src/scene.h
    ${CMAKE_CURRENT_LIST_DIR}/src/ImGuizmo/ImGuizmo.h
    ${CMAKE_CURRENT_LIST_DIR}/src/imgui/cimgui.h
    ${CMAKE_CURRENT_LIST_DIR}/src/imgui/cimguizmo.h
)

set(INCLUDE ${APP_INCLUDE} ${CORE_INCLUDE})

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
)

set(ASSETS
    assets/DGLogo.png
)

set(ALL_ASSETS ${ASSETS} ${SHADERS})
set(CORE ${CORE_SOURCE} ${CORE_INCLUDE})

source_group("assets" FILES ${ALL_ASSETS})  
source_group("core" FILES ${CORE})  


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

