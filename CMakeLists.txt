############
### Project Setup
############
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(NeuSneM)

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

aux_source_directory(src SRCS)

add_executable(NeuSneM ${HEADERS} ${SRCS} ${ASM_SRCS})
target_link_libraries(NeuSneM ${ALLEGRO_LIBS})
#target_compile_features(NeuSneM PRIVATE cxx_range_for)
