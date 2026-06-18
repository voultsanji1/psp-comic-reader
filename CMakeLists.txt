cmake_minimum_required(VERSION 3.10)

project(PSPComicReader VERSION 1.0.0)

# PSP SDK path
set(PSPSDK $ENV{PSPSDK})
if(NOT PSPSDK)
    message(FATAL_ERROR "PSPSDK environment variable not set")
endif()

set(CMAKE_SYSTEM_NAME PSP)
set(CMAKE_C_COMPILER   psp-gcc)
set(CMAKE_CXX_COMPILER psp-g++)

# Compiler flags
set(PSP_CFLAGS "-G0 -Wall -O2 -fno-strict-aliasing")
set(CMAKE_C_FLAGS   "${PSP_CFLAGS}")
set(CMAKE_CXX_FLAGS "${PSP_CFLAGS} -std=c++17 -fno-exceptions -fno-rtti")

# Include directories
include_directories(
    ${PSPSDK}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libs/miniz
    ${CMAKE_SOURCE_DIR}/src/libs/libjpeg
)

# Source files
set(SOURCES
    src/main.cpp
    src/display.cpp
    src/input.cpp
    src/archive.cpp
    src/image.cpp
    src/browser.cpp
    src/bookmarks.cpp
    src/settings.cpp
    src/ui.cpp
    src/libs/miniz/miniz.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Link PSP libraries
target_link_libraries(${PROJECT_NAME}
    -L${PSPSDK}/lib
    -lpspgu
    -lpspgum
    -lpspctrl
    -lpspdisplay
    -lpspge
    -lpspvfpu
    -lpsputility
    -lpsppower
    -lpspaudio
    -lpspaudiolib
    -lm
    -lc
    -lg
    -lstdc++
)

# PSP-specific build steps
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND psp-fixup-imports ${PROJECT_NAME}
    COMMAND mksfoex -d MEMSIZE=1 "${PROJECT_NAME}" PARAM.SFO
    COMMAND pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL ${PROJECT_NAME} NULL
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building PSP EBOOT.PBP"
)
