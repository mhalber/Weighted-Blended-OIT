cmake_minimum_required( VERSION 2.8 )

project( OIT )

find_package( OpenGL REQUIRED )
find_package( GLFW3 REQUIRED )

include_directories( "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/extern" )

add_executable( OIT main.c extern/glad.c )
target_link_libraries( OIT glfw3 ${OPENGL_gl_LIBRARY} )
target_compile_options( OIT PRIVATE -std=c11 -Wall )