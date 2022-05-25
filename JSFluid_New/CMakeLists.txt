project( JSFluid_New )

include_directories( . )

file( GLOB SRCS "*.cpp" )
file( GLOB INCS "*.h" )

source_group( Sources FILES ${SRCS} ${INCS} )

add_executable( ${PROJECT_NAME} ${SRCS} ${INCS} )

target_link_libraries( ${PROJECT_NAME}
    ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${PLATFORM_LINK_LIBS} )

Copy_DLLs_to_RuntimeOut()

