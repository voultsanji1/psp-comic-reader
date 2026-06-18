# PSP cross-compilation toolchain for CMake
# Used when PSPSDK doesn't ship its own psp-toolchain.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

find_program(PSP_GCC   psp-gcc   REQUIRED)
find_program(PSP_GPP   psp-g++   REQUIRED)
find_program(PSP_AR    psp-ar    REQUIRED)
find_program(PSP_OBJCOPY psp-objcopy REQUIRED)

set(CMAKE_C_COMPILER   ${PSP_GCC})
set(CMAKE_CXX_COMPILER ${PSP_GPP})
set(CMAKE_AR           ${PSP_AR})

set(CMAKE_C_FLAGS_INIT   "-G0 -Wall -O2 -fno-strict-aliasing -DPSP")
set(CMAKE_CXX_FLAGS_INIT "-G0 -Wall -O2 -fno-strict-aliasing -DPSP -std=c++17 -fno-exceptions -fno-rtti")

set(CMAKE_FIND_ROOT_PATH $ENV{PSPSDK})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
