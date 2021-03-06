cmake_minimum_required (VERSION 3.0.2)

set(CMAKE_C_COMPILER_INIT g++)
enable_testing()


project (HybridDBT)

 
# add the binary tree to the search path for include files
include_directories(./includes)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ggdb -std=c++11 -D__LINUX_API -D__USE_AC -D AC_NOT_USING_INTN -pthread")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)
set (EXECUTABLE  "")

add_subdirectory(src)

#add_executable(dumpBinaryBytecode ${SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/dumpBinaryBytecode.cpp)
#add_executable(dumpBinaryCode ${SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/dumpBinaryCode.cpp)
#add_executable(dumpResultsForAcceleratedSimulation ${SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/dumpResultsForAcceleratedSimulation.cpp)
#add_executable(readBytecode ${SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/readBytecode.cpp)
#add_executable(dbt ${SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/src/dbt/dbt.cpp)

foreach(oneExecutable ${EXECUTABLE})
	get_filename_component(execName ${oneExecutable} NAME_WE)

  if (execName MATCHES "dbt")
    add_executable(dbt ${SOURCE} ${oneExecutable})
    set_target_properties(dbt PROPERTIES COMPILE_DEFINITIONS "IR_SUCC")
    
    add_executable(dbtScoreboard ${SOURCE} ${oneExecutable})
  else()
	  add_executable(${execName} ${SOURCE} ${oneExecutable})
  endif()

endforeach(oneExecutable)

#include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
#ExternalProject_Add(benchmarks SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/benchmarks BINARY_DIR ./benchmarks/bin INSTALL_COMMAND ls)

add_subdirectory(benchmarks)
