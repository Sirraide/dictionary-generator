cmake_minimum_required(VERSION 3.27)
project(dictionary-generator VERSION 0.1.0 LANGUAGES CXX)

## ============================================================================
##  Global CMake Variables.
## ============================================================================
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## ============================================================================
##  Global compiler options.
## ============================================================================
## Turn on diagnostics colours.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

## Use mold as the default linker, if it exists.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    find_program(MOLD_LINKER "mold")
    if (MOLD_LINKER)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-fuse-ld=mold)
        endif()
        add_link_options(-fuse-ld=mold)
    endif()
endif()

## ============================================================================
##  Compiler options.
## ============================================================================
add_library(_dictionary_generator_options INTERFACE)

## Flags for Clang and GCC.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(_dictionary_generator_options INTERFACE
        ## Warnings.
        -Wall -Wextra     # Enable ‘all’ warnings.
        -Wundef           # Invalid #undef or undefined macro in #if.
        -Wcast-align      # Casting that changes alignment.
        -Wconversion      # Implicit conversions.
        -Wsign-conversion # Implicit sign conversions.
        -Wformat=2        # Stricter format checking.

        ## Disabled warnings.
        -Wno-unused-function
        -Wno-unused-local-typedefs

        ## NULL Errors.
        -Werror=nonnull # Passing NULL to nonnull parameter.

        ## Memory Errors.
        -Werror=address              # Suspicious use of addresses.
        -Werror=init-self            # Initialization of a variable with itself.
        -Werror=uninitialized

        ## Return type.
        -Werror=return-type
        -Wmissing-noreturn

        ## C/C++.
        -Werror=implicit-fallthrough
        -Werror=pointer-arith        # Disallow void* and function pointer arithmetic.
        -Werror=string-compare       # Nonsensical string comparisons.
        -Werror=switch               # Missing switch cases.
        # -Werror=switch-enum          # Switch on enum (even if there is a default case).
        -Werror=write-strings        # Strings in C should be const char*.

        ## C++.
        -Werror=missing-field-initializers
        -Werror=non-virtual-dtor
        -Werror=pessimizing-move
    )
endif()

## Additional flags for GCC.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(_dictionary_generator_options INTERFACE
        -Wlogical-op      # Duplicate or unintended logical operators.
        -Werror=invalid-memory-model # For atomics.
        -Werror=maybe-uninitialized
        -Werror=missing-requires
        -Werror=return-local-addr
    )
endif()

## Additional flags for Clang.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(_dictionary_generator_options INTERFACE
        -Werror=dangling
        -Werror=return-stack-address
    )
endif()

## Flags for MSVC.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(_dictionary_generator_options INTERFACE
        /W4 # Enable ‘all’ warnings.

        # Allow unnamed structs/unions.
        /wd4201

        # Source code is UTF-8.
        /utf-8
    )
endif()

## On Windows, don’t suggest the _s nonsense functions.
if (WIN32)
    target_compile_definitions(_dictionary_generator_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS_GLOBALS
        _CRT_NONSTDC_NO_WARNINGS
    )
endif()

## Address Sanitiser.
if (ENABLE_ASAN)
    target_compile_options(_dictionary_generator_options INTERFACE -fsanitize=address)
    target_link_options(_dictionary_generator_options INTERFACE -fsanitize=address)
endif()

## Debug/Release flags.
if (NOT MSVC)
    target_compile_options(_dictionary_generator_options INTERFACE
        $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3 -D_GLIBCXX_ASSERTIONS>
        $<$<CONFIG:RELEASE>:-O3 -march=native>
    )
    target_link_options(_dictionary_generator_options INTERFACE
        $<$<CONFIG:DEBUG>:-O0 -g3 -ggdb3 -rdynamic>
        $<$<CONFIG:RELEASE>:-O3 -march=native>
    )
else()
    target_compile_options(_dictionary_generator_options INTERFACE
        $<$<CONFIG:DEBUG>:/Od>
        $<$<CONFIG:RELEASE>:/O2>
    )
endif()

## ============================================================================
##  Dependencies and include dirs.
## ============================================================================
# ‘src’ should be an include directory.

## Add libbase.
include(FetchContent)
FetchContent_Declare(base
    GIT_REPOSITORY https://github.com/Sirraide/libbase
    GIT_TAG master
)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(base json)

## ============================================================================
##  Executables and libraries.
## ============================================================================
file(GLOB_RECURSE sources src/*.cc)
file(GLOB_RECURSE headers include/*.hh src/*.hh)

## Add the executable.
add_library(dictionary-generator STATIC ${sources})
target_sources(dictionary-generator PUBLIC FILE_SET HEADERS FILES ${headers})
target_include_directories(dictionary-generator PUBLIC include)

## Apply our options.
target_link_libraries(dictionary-generator
    PUBLIC libbase
    PRIVATE _dictionary_generator_options nlohmann_json::nlohmann_json
)


## ============================================================================
##  Testing
## ============================================================================
if (DEFINED BUILD_DICT_GEN_TESTS)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.6.0
    )
    FetchContent_MakeAvailable(Catch2)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)

    file(GLOB_RECURSE test_sources test/*.cc)
    add_executable(tests ${test_sources})
    target_link_libraries(tests PRIVATE dictionary-generator Catch2::Catch2WithMain)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(tests PRIVATE -fms-extensions -fdeclspec)
    endif()

    target_compile_definitions(tests PRIVATE
        "LIBBASE_TESTING_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\""
        "LIBBASE_IS_BUILDING_TESTS"
    )

    catch_discover_tests(tests)
endif()
