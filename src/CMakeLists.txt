# ThreadInstrument: Library to monitor thread activity
# Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
#
# Distributed under the MIT License. (See accompanying file LICENSE)

# \file     CMakeLists.txt
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8 )

add_library( thread_instrument STATIC thread_instrument.cpp )
target_include_directories( thread_instrument PUBLIC ${PROJECT_SOURCE_DIR}/include )

#install

install( TARGETS thread_instrument
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib )

install( DIRECTORY ../include/thread_instrument
         DESTINATION include
         FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" )
