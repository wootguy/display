cmake_minimum_required(VERSION 3.6)

set(SVEN_ROOT_PATH "C:/Games/Steam/steamapps/common/Sven Co-op/" CACHE FILEPATH "Where your Sven Co-op folder is")
set(DLL_INSTALL_PATH "svencoop/addons/metamod/dlls" CACHE FILEPATH "Where to install the DLL (relative to Sven Co-op folder)")

project(MediaPlayer)

set(SOURCE_FILES
	src/main.h src/main.cpp
	src/subprocess.h 
	src/lodepng.h src/lodepng.cpp
	src/Display.h src/Display.cpp
	src/mp3.h src/mp3.cpp
	src/SteamVoiceEncoder.h src/SteamVoiceEncoder.cpp
	src/crc32.h src/crc32.cpp
	src/AudioPlayer.h src/AudioPlayer.cpp
	src/VideoPlayer.h src/VideoPlayer.cpp
)

if(MSVC)											
	set(SOURCE_FILES ${SOURCE_FILES} src/subprocess_windows.cpp)
else()
	set(SOURCE_FILES ${SOURCE_FILES} src/subprocess_linux.cpp)
endif()

add_library(${PROJECT_NAME} SHARED)
add_subdirectory(mmlib)
include_directories(include)

if(MSVC)
	source_group("Header Files\\sven" FILES ${SVEN_API_HEADERS})
	source_group("Header Files\\mmlib" FILES ${META_HELPER_HEADERS})
	source_group("Source Files\\mmlib" FILES ${META_HELPER_SOURCES})
	
	set_target_properties(${PROJECT_NAME} PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SVEN_ROOT_PATH}/${DLL_INSTALL_PATH}"
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SVEN_ROOT_PATH}/${DLL_INSTALL_PATH}")
					  
	set_target_properties(${PROJECT_NAME} PROPERTIES
					  VS_STARTUP_PROJECT ${PROJECT_NAME}
					  VS_DEBUGGER_COMMAND "svencoop.exe"
					  VS_DEBUGGER_COMMAND_ARGUMENTS "+map stadium3 +maxplayers 32 +developer 1 -nomsaa -nofbo -noborder -condebug -dll addons/metamod/dlls/metamod.dll"
					  VS_DEBUGGER_WORKING_DIRECTORY "${SVEN_ROOT_PATH}")
					  
	# can't overwrite dll while its loaded but it can be renamed, then you can run "meta reload <plugin>" to update it in-game				
	add_custom_command(TARGET ${PROJECT_NAME}  PRE_BUILD
		COMMAND ${CMAKE_CURRENT_LIST_DIR}/replace_dll.bat
	)
	
	target_link_libraries(${PROJECT_NAME}
		${CMAKE_CURRENT_LIST_DIR}/lib/opus.lib
	)

else()
	set(CMAKE_CXX_FLAGS "-Wall -std=c++11 -m32 -g -fvisibility=hidden")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0")
	set(CMAKE_CXX_FLAGS_RELEASE "-Os -fno-exceptions -w -Wfatal-errors")
	target_compile_definitions(${PROJECT_NAME} PUBLIC -Dlinux)
	
	target_link_libraries(${PROJECT_NAME} -lopus)
endif()
