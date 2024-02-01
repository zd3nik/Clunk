cmake_minimum_required(VERSION 3.1)
include(init.cmake)

#-----------------------------------------------------------------------------
# Build senjo lib
#
# Ordinarily this would be done local to the senjo source directory.
# But since senjo is a git submodule I don't want to add a CMakeLists.txt
# to it because it may conflict with the cmake setup of any project that
# wishes to use it.
#
# So I'll just do everything here in this top-level CMakeLists.txt
#-----------------------------------------------------------------------------
project(senjo CXX)
set(OBJ_HDR
    src/senjo/BackgroundCommand.h
    src/senjo/ChessMove.h
    src/senjo/MoveFinder.h
    src/senjo/Platform.h
    src/senjo/Threading.h
    src/senjo/ChessEngine.h
    src/senjo/EngineOption.h
    src/senjo/Output.h
    src/senjo/Square.h
    src/senjo/UCIAdapter.h
)
set(OBJ_SRC
    src/senjo/BackgroundCommand.cpp
    src/senjo/ChessEngine.cpp
    src/senjo/EngineOption.cpp
    src/senjo/MoveFinder.cpp
    src/senjo/Output.cpp
    src/senjo/Threading.cpp
    src/senjo/UCIAdapter.cpp
)

include_directories(src/senjo)
add_library(${PROJECT_NAME} STATIC ${OBJ_HDR} ${OBJ_SRC})
if(CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

#-----------------------------------------------------------------------------
# Build clunk
#
# This would ordinarily be done in a CMakeLists.txt local to the clunk source
# directory.  See comment above, in the build senjo section.
#-----------------------------------------------------------------------------
project(clunk CXX)
set(OBJ_HDR
    src/Clunk.h
    src/Defs.h
    src/HashTable.h
    src/Move.h
    src/Stats.h
    src/Types.h
)
set(OBJ_SRC
    src/Clunk.cpp
    src/HashTable.cpp
    src/Stats.cpp
)

include_directories(src src/senjo)
add_executable(${PROJECT_NAME} ${OBJ_HDR} ${OBJ_SRC} src/main.cpp)
target_link_libraries(${PROJECT_NAME} senjo)

add_custom_command(
    TARGET ${PROJECT_NAME}
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/epd" "${CMAKE_BINARY_DIR}/epd"
    COMMENT "Copying EPD test files"
)

