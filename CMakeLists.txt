cmake_minimum_required(VERSION 3.10)

# ── Rileva PSPSDK prima di tutto ─────────────────────────────────────────────
if(NOT DEFINED ENV{PSPSDK})
    message(FATAL_ERROR "La variabile PSPSDK non è impostata. Esegui: export PSPSDK=$(psp-config --pspsdk-path)")
endif()

set(PSPSDK_PATH "$ENV{PSPSDK}")
set(PSPDEV_PATH "$ENV{PSPDEV}")

# ── Toolchain PSP ─────────────────────────────────────────────────────────────
# Imposta il sistema target PRIMA di project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

find_program(PSP_GCC psp-gcc REQUIRED)
find_program(PSP_GPP psp-g++ REQUIRED)
set(CMAKE_C_COMPILER   ${PSP_GCC})
set(CMAKE_CXX_COMPILER ${PSP_GPP})

# Evita che CMake testi il compilatore su host (cross-compile)
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(PSPComicReader VERSION 1.0.0 LANGUAGES C CXX)

# ── Flag di compilazione ──────────────────────────────────────────────────────
set(PSP_CFLAGS   "-G0 -Wall -O2 -fno-strict-aliasing -DPSP -D_PSP_FW_VERSION=150")
set(CMAKE_C_FLAGS   "${PSP_CFLAGS}")
set(CMAKE_CXX_FLAGS "${PSP_CFLAGS} -std=c++17 -fno-exceptions -fno-rtti")

# ── Include ───────────────────────────────────────────────────────────────────
include_directories(
    ${PSPSDK_PATH}/include           # pspkernel.h, pspgu.h, pspctrl.h …
    ${PSPDEV_PATH}/psp/sdk/include   # fallback per alcune installazioni
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libs/miniz
    ${CMAKE_SOURCE_DIR}/src/libs/stb
)

# ── Librerie di link ──────────────────────────────────────────────────────────
link_directories(
    ${PSPSDK_PATH}/lib
    ${PSPDEV_PATH}/psp/sdk/lib
)

# ── Sorgenti ──────────────────────────────────────────────────────────────────
# NOTA: miniz è incluso come header-only via miniz.h (vedi src/libs/miniz/miniz.h),
# quindi miniz.c NON viene compilato come unità separata (evita doppia definizione).
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
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    pspgu
    pspgum
    pspvfpu
    pspctrl
    pspdisplay
    pspge
    psputility
    psppower
    psprtc
    pspaudio
    m
    c
    stdc++
    g
)

# ── Step post-build PSP ───────────────────────────────────────────────────────
# Trova gli strumenti PSP
find_program(PSP_FIXUP  psp-fixup-imports REQUIRED)
find_program(MKSFO      mksfoex           REQUIRED)
find_program(PACK_PBP   pack-pbp          REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${PSP_FIXUP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${MKSFO} -d MEMSIZE=1 "PSPComicReader" PARAM.SFO
    COMMAND ${PACK_PBP} EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL
            $<TARGET_FILE:${PROJECT_NAME}> NULL
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Impacchettamento EBOOT.PBP..."
)
