include(common RESULT_VARIABLE RES)
if(NOT RES)
	message(FATAL_ERROR "common.cmake not found. Should be in {repo_root}/cmake directory")
endif()

set(KRIS_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/kris_common.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/renderer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/resource_allocator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/cmd_recorder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/material.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/material.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/material_builder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/mesh.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/scene.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/passes/base_pass.cpp"
)
set(KRIS_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/kris_common.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/renderer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/resource_allocator.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/resource_utils.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/cmd_recorder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/material.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/material_builder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/mesh.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/scene.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/passes/pass_common.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/kris/passes/base_pass.h"
)

nbl_create_executable_project("${KRIS_SOURCES}" "" "${KRIS_INCLUDES}" "" "${NBL_EXECUTABLE_PROJECT_CREATION_PCH_TARGET}")