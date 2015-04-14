############
### Project Setup
############
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(NeuSneM)
# On some platforms, CMake automatically adds some libraries to this by
# default (e.g. Windows) which we don't need, so unset it.
unset(CMAKE_C_STANDARD_LIBRARIES)

if(ALLEGRO_INSTALL_PREFIX)
	link_directories(${ALLEGRO_INSTALL_PREFIX}/lib)
	include_directories(${ALLEGRO_INSTALL_PREFIX}/include)
	set(ALLEGRO_LIBS alleg)
else()
	exec_program("allegro-config" ARGS "--libs" OUTPUT_VARIABLE ALLEGRO_LIBS
		RETURN_VALUE ALLEGRO_STATUS)
	if(NOT ${ALLEGRO_STATUS} EQUAL 0)
		message(FATAL_ERROR "ALLEGRO_INSTALL_PREFIX unset, and allegro-config was not found or failed to run.")
	endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -Wall -Wno-unused-but-set-variable")
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=pe-i386")

aux_source_directory(src SRCS)
set(RC_FILE src/snem.rc)

add_executable(NeuSneM ${SRCS} ${RC_FILE})
target_link_libraries(NeuSneM ${ALLEGRO_LIBS} winmm)
set_target_properties(NeuSneM PROPERTIES LINK_FLAGS "-m32")
