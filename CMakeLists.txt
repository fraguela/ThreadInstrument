# ThreadInstrument: Library to monitor thread activity
# Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
#
# Distributed under the MIT License. (See accompanying file LICENSE)

# \file     CMakeLists.txt
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8 )

project( ThreadInstrument )

set( CMAKE_CXX_STANDARD 11 )

add_subdirectory( src )
add_subdirectory( tests )

find_package(Doxygen)
if(DOXYGEN_FOUND)
  
  set(DOXY_DEPS
    ${PROJECT_SOURCE_DIR}/src/thread_instrument.cpp
    ${PROJECT_SOURCE_DIR}/include/thread_instrument/thread_instrument.h
  )
  
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/doc
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/doc ${CMAKE_CURRENT_BINARY_DIR}/doc
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/doc
    MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/doc/Doxyfile
    DEPENDS ${DOXY_DEPS}
    COMMENT "Generating doxygen documentation"
  )
	
  add_custom_target(doc ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html)
  
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/ DESTINATION share/thread_instrument/doc)

  # list( APPEND ADDITIONAL_MAKE_CLEAN_FILES ${ADDITIONAL_MAKE_CLEAN_FILES} ${PROJECT_SOURCE_DIR}/doc/html ${PROJECT_SOURCE_DIR}/doc/latex )

endif(DOXYGEN_FOUND)
