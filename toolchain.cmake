#
# Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/cppalliance/mrdox
#

set(BUILD_SHARED_LIBS OFF CACHE STRING "")
set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON CACHE STRING "")

# Windows, Win64
if(WIN32)
    add_definitions(
        -D_WIN32_WINNT=0x0601
        -D_CRT_SECURE_NO_WARNINGS
    )
endif()

# x86
if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32") # 32-bit
    add_compile_options(
        /arch:SSE2
    )
endif()
