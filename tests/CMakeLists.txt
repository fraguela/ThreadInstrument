# ThreadInstrument: Library to monitor thread activity
# Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
#
# Distributed under the MIT License. (See accompanying file LICENSE)

# \file     CMakeLists.txt
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8 )

include_directories( ${PROJECT_SOURCE_DIR}/include )
add_definitions( -DTHREADINSTRUMENT )
link_libraries( thread_instrument )

# Tests based on C++11 threads
set(tests pfor pfor_simpl pfor_simpl2 pforlog pforlog_simpl bench )

foreach(test ${tests})
  add_executable( ${test} ${test}.cpp )
  target_link_libraries( ${test} pthread )
endforeach(test)

#Tests based on OpenMP
find_package( OpenMP QUIET )

if( OPENMP_FOUND OR OpenMP_CXX_FLAGS )

  message(STATUS "Found OpenMP")
  if( NOT OPENMP_FOUND )
    message(STATUS " although OPENMP_FOUND is not set...")
  endif(  NOT OPENMP_FOUND )
 
  set( tests_openmp pfor_openmp pforlog_openmp )
  
  foreach(test ${tests_openmp})
    add_executable( ${test} ${test}.cpp )
    target_compile_options( ${test} PUBLIC ${OpenMP_CXX_FLAGS} )
    target_link_libraries( ${test} ${OpenMP_CXX_FLAGS} )
    #set( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}" )
  endforeach(test)
 
  find_package( OpenBLAS QUIET 
    PATHS $ENV{HOME}/local/lib )
  
  if(OpenBLAS_FOUND)
    message(STATUS "Found OpenBLAS ")
    set(BLAS_LIBRARIES ${OpenBLAS_LIBRARIES})
    set(BLAS_INCLUDE_DIRS ${OpenBLAS_INCLUDE_DIRS})
    set(BLAS_FOUND ON)
  else(OpenBLAS_FOUND)
    set(BLA_STATIC ON)
    find_package(BLAS QUIET)
  endif(OpenBLAS_FOUND)
 
  if(BLAS_FOUND)
    message(STATUS "Found BLAS -I: ${OpenBLAS_INCLUDE_DIRS} -L: ${BLAS_LINKER_FLAGS} ${BLAS_LIBRARIES} ")
    set( tests_openmp_blas matrix_inverse_omp matrix_inverse_omp_simpl )
    foreach(test ${tests_openmp_blas})
      add_executable( ${test} ${test}.cpp ) 
      target_compile_options( ${test} PUBLIC ${OpenMP_CXX_FLAGS} )
      target_link_libraries( ${test} ${OpenMP_CXX_FLAGS} )
      target_compile_definitions( ${test} PRIVATE NOSERIAL NOTEST )
      target_include_directories( ${test} PRIVATE ${OpenBLAS_INCLUDE_DIRS} )
      target_link_libraries( ${test} ${BLAS_LINKER_FLAGS} ${BLAS_LIBRARIES} )
    endforeach(test)
  endif(BLAS_FOUND)

else( OPENMP_FOUND OR OpenMP_CXX_FLAGS )
  message(STATUS "OpenMP not found. Tests skipped...")
endif( OPENMP_FOUND OR OpenMP_CXX_FLAGS )

# Tests based on Intel TBB
find_library( TBB_LIBRARY tbb 
  PATHS $ENV{HOME}/tbb/lib $ENV{HOME}/local/lib )

mark_as_advanced( TBB_LIBRARY )

if( TBB_LIBRARY )
  message(STATUS "Found tbb lib: ${TBB_LIBRARY}")
  get_filename_component( TBB_LIB_DIR ${TBB_LIBRARY} DIRECTORY )
  get_filename_component( TBB_INCL_LIB_DIR ${TBB_LIB_DIR}/../include ABSOLUTE)
  find_path(TBB_INCLUDE_DIR tbb/tbb.h 
    HINTS ${TBB_INCL_LIB_DIR} 
    PATHS $ENV{HOME}/tbb/include $ENV{HOME}/local/include  )
  message(STATUS "Found tbb include dir: ${TBB_INCLUDE_DIR}")
  mark_as_advanced( TBB_INCLUDE_DIR )

  if(${APPLE})
    #The reason is that in OSX El Capitan (10.11) we need cmake -E env
    cmake_minimum_required( VERSION 3.1.3 )
    set( CMAKE_MACOSX_RPATH ON )
  endif(${APPLE})

  set( tests_tbb pfor_tbb pforlog_tbb )
  foreach(test ${tests_tbb})
    add_executable( ${test} ${test}.cpp )
    target_include_directories( ${test} PUBLIC ${TBB_INCLUDE_DIR} )
    target_link_libraries( ${test} ${TBB_LIBRARY} )
  endforeach(test)
else( TBB_LIBRARY )
  message(STATUS "TBB not found. Tests skipped..")
endif( TBB_LIBRARY )

list( APPEND tests ${tests_openmp} )
list( APPEND tests ${tests_tbb} )

add_custom_target(check)
foreach(test ${tests})
  add_custom_command(TARGET check PRE_BUILD
                     COMMAND ${CMAKE_COMMAND} -E echo test ${test} : -------------
                     COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$ENV{LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH} ${test} || exit 1
                     COMMAND ${CMAKE_COMMAND} -E echo ------------------------
                    )
endforeach(test)
add_custom_command(TARGET check PRE_BUILD COMMAND echo ${tests} TESTS DONE. SUCCESSFUL)
