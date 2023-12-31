cmake_minimum_required (VERSION 3.6)
project(gainput)
set(GAINPUT_MAJOR_VERSION 1)
set(GAINPUT_MINOR_VERSION 0)
set(GAINPUT_PATCH_VERSION 0)
set(GAINPUT_VERSION ${GAINPUT_MAJOR_VERSION}.${GAINPUT_MINOR_VERSION}.${GAINPUT_PATCH_VERSION})

option(GAINPUT_SAMPLES "Build Samples for Gainput" OFF)
option(GAINPUT_TESTS "Build Tests for Gainput" OFF)
option(GAINPUT_BUILD_SHARED "BUILD_SHARED" OFF)
option(GAINPUT_BUILD_STATIC "BUILD_STATIC" ON)

if(!WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")
else()
  set(XINPUT "Xinput9_1_0")
  if ( ${CMAKE_SYSTEM_VERSION} LESS 6.1 )
    set(XINPUT, "xinput")
  endif()
endif()

message(STATUS "GAINPUT version ${GAINPUT_VERSION}")

set(CMAKE_MACOSX_RPATH 1)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -Wall -Wextra -pedantic -Wshadow -Wno-variadic-macros")
endif()

include_directories (include/)

file(GLOB_RECURSE sources source/*.cpp source/*.h include/*.h)

if(APPLE)
	file(GLOB_RECURSE mmsources source/*.mm)
endif()

## build STATIC *or* SHARED
if (GAINPUT_BUILD_SHARED)
  message(STATUS "..Building shared libraries (-DGAINPUT_BUILD_SHARED=OFF to disable)")
  add_library(gainput SHARED ${sources} ${mmsources})
  set_target_properties(gainput PROPERTIES
    OUTPUT_NAME gainput
    DEBUG_POSTFIX -d
    VERSION ${GAINPUT_VERSION}
    SOVERSION ${GAINPUT_MAJOR_VERSION}
    FOLDER gainput
  )
  set(install_libs ${install_libs} gainput)
endif (GAINPUT_BUILD_SHARED)

if (GAINPUT_BUILD_STATIC)
  message(STATUS "..Building static libraries (-DGAINPUT_BUILD_STATIC=OFF to disable)")
  add_library(gainputstatic STATIC ${sources} ${mmsources})
  set_target_properties(gainputstatic PROPERTIES DEBUG_POSTFIX -d FOLDER gainput)
  set(install_libs ${install_libs} gainputstatic)

  target_include_directories(gainputstatic INTERFACE 3rdparty/gainput/include)
endif (GAINPUT_BUILD_STATIC)

if(WIN32)
	#target_link_libraries(gainput ${XINPUT} ws2_32)
	target_link_libraries(gainputstatic ${XINPUT} ws2_32)
	add_definitions(-DGAINPUT_LIB_DYNAMIC=1)
elseif(ANDROID)
	target_link_libraries(gainputstatic native_app_glue log android)
	target_link_libraries(gainput native_app_glue log android)
elseif(APPLE)
	find_library(FOUNDATION Foundation)
	find_library(IOKIT IOKit)
  find_library(GAME_CONTROLLER GameController)
	target_link_libraries(gainput ${FOUNDATION} ${IOKIT} ${GAME_CONTROLLER})
  if(IOS)
      find_library(UIKIT UIKit)
      find_library(COREMOTION CoreMotion)
      find_library(QUARTZCORE QuartzCore)
      target_link_libraries(gainput ${UIKIT} ${COREMOTION})
  else()
      find_library(APPKIT AppKit)
      target_link_libraries(gainput ${APPKIT})
  endif()
endif()



# Library installation directory
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT DEFINED CMAKE_INSTALL_LIBDIR)
set(libdir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

install(
    DIRECTORY "include/gainput"
    DESTINATION "include"
    FILES_MATCHING PATTERN "*.h"
)

install(
    TARGETS ${install_libs}
    LIBRARY DESTINATION "${libdir}"
    ARCHIVE DESTINATION "${libdir}"
    RUNTIME DESTINATION "bin"
)
