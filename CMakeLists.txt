include(common RESULT_VARIABLE RES)
if(NOT RES)
	message(FATAL_ERROR "common.cmake not found. Should be in {repo_root}/cmake directory")
endif()

set(KRIS_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/kris_common.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/renderer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/resource_allocator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmd_recorder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/material_builder.cpp"
)
set(KRIS_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/kris_common.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/renderer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/resource_allocator.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmd_recorder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/material.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/material_builder.h"
)

nbl_create_executable_project("${KRIS_SOURCES}" "" "${KRIS_INCLUDES}" "" "${NBL_EXECUTABLE_PROJECT_CREATION_PCH_TARGET}")