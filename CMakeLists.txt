cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.16)

project(rlbox_wasm2c
        VERSION 0.1
        DESCRIPTION "RLBox integration with WASM modules compiled with wasm2c")

# Project Settings ###################

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEV "Use settings suitable for dev contributions to rlbox" OFF)

file(GLOB_RECURSE
     ALL_CXX_SOURCE_FILES
     ${CMAKE_SOURCE_DIR}/include/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/include/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/include/*.cc
     ${CMAKE_SOURCE_DIR}/include/*.hh
     ${CMAKE_SOURCE_DIR}/include/*.ii
     ${CMAKE_SOURCE_DIR}/include/*.[CHI]
     ${CMAKE_SOURCE_DIR}/test/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/test/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/test/*.cc
     ${CMAKE_SOURCE_DIR}/test/*.hh
     ${CMAKE_SOURCE_DIR}/test/*.ii
     ${CMAKE_SOURCE_DIR}/test/*.[CHI]
     ${CMAKE_SOURCE_DIR}/c_src/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/c_src/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/c_src/*.cc
     ${CMAKE_SOURCE_DIR}/c_src/*.hh
     ${CMAKE_SOURCE_DIR}/c_src/*.ii
     ${CMAKE_SOURCE_DIR}/c_src/*.[CHI])

# Dev Tools ###################

if(DEV)
  find_program(CLANG_TIDY "clang-tidy")
  if(CLANG_TIDY)
    # Config in .clang-tidy
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
  endif()

  find_program(CLANG_FORMAT "clang-format")
  if(CLANG_FORMAT)
    # Config in .clang-format
    add_custom_target(format-source
                      COMMAND clang-format
                              -i
                              -style=file
                              ${ALL_CXX_SOURCE_FILES})
  endif()

endif()

# Dependencies ###################

find_program(CARGO "cargo")
if(!CARGO)
  message(
    FATAL_ERROR
      "Could not find cargo. Please instal/l cargo as it is needed to build rust libraries."
    )
endif()

find_program(EMCC "emcc")
if(!EMCC)
  message(
    FATAL_ERROR
      "Could not find emcc. Please install emcc as it is needed to generate the wasm2c sandbox."
    )
endif()

include(FetchContent)

FetchContent_Declare(
  rlbox
  GIT_REPOSITORY https://github.com/PLSysSec/rlbox_api_cpp17.git)
FetchContent_GetProperties(rlbox)
if(NOT rlbox_POPULATED)
  FetchContent_Populate(rlbox)
endif()

FetchContent_Declare(catch2
                     GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                     GIT_TAG v2.13.9)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
  FetchContent_Populate(catch2)
endif()

add_subdirectory("${catch2_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/contrib")

if (WIN32)
  FetchContent_Declare(
    wasiclang
    URL
      https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk-12.0-mingw.tar.gz
    )
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  FetchContent_Declare(
    wasiclang
    URL
      https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk-12.0-macos.tar.gz
    )
else()
  FetchContent_Declare(
    wasiclang
    URL
      https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk-12.0-linux.tar.gz
    )
endif()
FetchContent_GetProperties(wasiclang)
if(NOT wasiclang_POPULATED)
  FetchContent_Populate(wasiclang)
endif()

# FetchContent_Declare(
#   mod_wasm2c
#   GIT_REPOSITORY https://github.com/PLSysSec/wasm2c_sandbox_compiler
#   GIT_TAG main)
# FetchContent_GetProperties(mod_wasm2c)
# if(NOT mod_wasm2c_POPULATED)
#   FetchContent_Populate(mod_wasm2c)
# endif()

set (mod_wasm2c_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../hfi_wasm2c_sandbox_compiler/")

# if(DEV)
#   if(MSVC)
#     set(RLBOX_SANITIZER_COMPILE_FLAGS)
#     set(RLBOX_SANITIZER_LINK_FLAGS "")
#   else()
#     set(RLBOX_SANITIZER_COMPILE_FLAGS -fsanitize=address -fsanitize=undefined)
#     set(RLBOX_SANITIZER_LINK_FLAGS -fsanitize=address -fsanitize=undefined)
#   endif()
# else()
#   set(RLBOX_SANITIZER_COMPILE_FLAGS "")
#   set(RLBOX_SANITIZER_LINK_FLAGS "")
# endif()

if(DEV)
  set(WASM2C_BUILD_DIR_SUFFIX "debug")
  set (WASM2C_BUILD_TYPE Debug)
else()
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(WASM2C_BUILD_DIR_SUFFIX "debug")
    set (WASM2C_BUILD_TYPE Debug)
  elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set(WASM2C_BUILD_DIR_SUFFIX "relwithdebinfo")
    set (WASM2C_BUILD_TYPE RelWithDebInfo)
  elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    set(WASM2C_BUILD_DIR_SUFFIX "minsizerel")
    set (WASM2C_BUILD_TYPE MinSizeRel)
  else()
    set(WASM2C_BUILD_DIR_SUFFIX "release")
    set (WASM2C_BUILD_TYPE Release)
  endif()
endif()

# wasm2c ###################
set(WASM2C_RUNTIME_SOURCE_DIR "${mod_wasm2c_SOURCE_DIR}/wasm2c/")
set(C_SOURCE_FILES "${CMAKE_SOURCE_DIR}/c_src/wasm2c_sandbox_wrapper.c")

# wasm2c hfi ###################

set(WASM2C_RUNTIME_DIR "${mod_wasm2c_SOURCE_DIR}/build_release_guardpages/")

set(GLUE_LIB_WASM_DIR "${CMAKE_BINARY_DIR}/wasm/")
set(GLUE_LIB_WASM "${GLUE_LIB_WASM_DIR}/glue_lib_wasm2c.wasm")
set(GLUE_LIB_H "${GLUE_LIB_WASM_DIR}/glue_lib_wasm2c.h")
set(GLUE_LIB_C "${GLUE_LIB_WASM_DIR}/glue_lib_wasm2c.c")

add_custom_command(OUTPUT "${GLUE_LIB_H}" "${GLUE_LIB_C}" "${GLUE_LIB_WASM}"
                   DEPENDS ${C_SOURCE_FILES}
                   WORKING_DIRECTORY "${mod_wasm2c_SOURCE_DIR}"
                   COMMAND ${CMAKE_COMMAND}
                           -DCMAKE_BUILD_TYPE=${WASM2C_BUILD_TYPE}
                           -S  .
                           -B ${WASM2C_RUNTIME_DIR}
                           -DWasmSafetyEnforcement=WASM_USE_HFI
                   COMMAND ${CMAKE_COMMAND}
                           --build ${WASM2C_RUNTIME_DIR}
                           --config ${WASM2C_BUILD_TYPE}
                           --parallel
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${GLUE_LIB_WASM_DIR}
                    COMMAND ${wasiclang_SOURCE_DIR}/bin/clang
                            --sysroot ${wasiclang_SOURCE_DIR}/share/wasi-sysroot/
                            -O3
                            -Wl,--export-all -Wl,--no-entry -Wl,--growable-table -Wl,--stack-first -Wl,-z,stack-size=1048576
                            -o ${GLUE_LIB_WASM}
                            ${CMAKE_SOURCE_DIR}/c_src/wasm2c_sandbox_wrapper.c
                            ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib/libtest.c
                    COMMAND ${WASM2C_RUNTIME_DIR}/wasm2c
                            -o ${GLUE_LIB_C}
                            ${GLUE_LIB_WASM}
                   COMMENT "Building wasm2c-hfi compiler, runtime and wasm sandboxed library")



# Tests ###################

# if(DEV)
#   add_compile_options(-Wall -Wextra -pedantic ${RLBOX_SANITIZER_COMPILE_FLAGS})
#   add_link_options(${RLBOX_SANITIZER_LINK_FLAGS})
# endif()

set(WASM2C_RUNTIME_CODE_C ${WASM2C_RUNTIME_SOURCE_DIR}/uvwasi-rt.c  ${WASM2C_RUNTIME_SOURCE_DIR}/wasm-rt-impl.c  ${WASM2C_RUNTIME_SOURCE_DIR}/wasm-rt-os-unix.c)

set(WASM2C_RUNTIME_CODE ${WASM2C_RUNTIME_CODE_C} ${WASM2C_RUNTIME_SOURCE_DIR}/wasm-rt-shadow.cpp)
list(FILTER WASM2C_RUNTIME_CODE EXCLUDE REGEX "${WASM2C_RUNTIME_SOURCE_DIR}/wasm-rt-wasi.c" )
list(FILTER WASM2C_RUNTIME_CODE EXCLUDE REGEX "${WASM2C_RUNTIME_SOURCE_DIR}/examples/.*" )

##############

add_library(glue_lib_static STATIC ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_static PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_static ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_static PUBLIC WASM_USE_HFI)

##############

add_library(glue_lib_static_hfiemu STATIC ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_static_hfiemu PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                  PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                  PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_static_hfiemu ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_static_hfiemu PUBLIC WASM_USE_HFI
                                                  PUBLIC HFI_EMULATION)
add_dependencies(glue_lib_static_hfiemu glue_lib_static)

##############

add_library(glue_lib_static_hfiemu2 STATIC ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_static_hfiemu2 PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                   PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                   PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_static_hfiemu2 ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_static_hfiemu2 PUBLIC WASM_USE_HFI
                                                   PUBLIC HFI_EMULATION2)
add_dependencies(glue_lib_static_hfiemu2 glue_lib_static_hfiemu)

##############

add_library(glue_lib_static_hfiemu3 STATIC ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_static_hfiemu3 PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                   PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                   PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_static_hfiemu3 ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_static_hfiemu3 PUBLIC WASM_USE_HFI
                                                   PUBLIC HFI_EMULATION3)
add_dependencies(glue_lib_static_hfiemu3 glue_lib_static_hfiemu2)

##############

add_library(glue_lib_static_fsgs STATIC ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_static_fsgs PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                   PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                   PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_static_fsgs ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_static_fsgs PUBLIC WASM_USE_SEGMENT)
add_dependencies(glue_lib_static_fsgs glue_lib_static_hfiemu3)

##############

add_library(glue_lib_so SHARED ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_so PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                       PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                       PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_so ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_so PUBLIC WASM_USE_HFI)

# Add a dependency to glue_lib_static so we don't invoke the generator for GLUE_LIB_C multiple times
add_dependencies(glue_lib_so glue_lib_static_fsgs)
# default firefox flags
# target_compile_options(glue_lib_so PRIVATE -Werror -Wall -Wempty-body -Wignored-qualifiers -Wpointer-arith -Wsign-compare -Wtype-limits -Wunreachable-code -Wduplicated-cond -Wno-error=maybe-uninitialized -Wno-error=deprecated-declarations -Wno-error=array-bounds -Wno-error=coverage-mismatch -Wno-error=free-nonheap-object -Wno-multistatement-macros -Wno-error=class-memaccess -Wno-error=deprecated-copy -Wno-error=unused-but-set-variable -Wformat -Wformat-overflow=2 -Wno-psabi -Wno-error=unused-function)

##############

add_library(glue_lib_so_shadow_asan SHARED ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_so_shadow_asan PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                   PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                   PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_so_shadow_asan ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_so_shadow_asan PRIVATE WASM_CHECK_SHADOW_MEMORY)
# Needed else both binaries invoke the custom command to generate ${GLUE_LIB_C}
add_dependencies(glue_lib_so_shadow_asan glue_lib_so)
target_compile_definitions(glue_lib_so_shadow_asan PUBLIC WASM_USE_HFI)

##############

add_library(glue_lib_so_shadow_hfiemu2_asan SHARED ${GLUE_LIB_C} ${WASM2C_RUNTIME_CODE})
target_include_directories(glue_lib_so_shadow_hfiemu2_asan PRIVATE ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                   PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                   PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
)
target_link_libraries(glue_lib_so_shadow_hfiemu2_asan ${WASM2C_RUNTIME_DIR}/third_party/uvwasi/libuvwasi_a.a -luv)
target_compile_definitions(glue_lib_so_shadow_hfiemu2_asan PRIVATE WASM_CHECK_SHADOW_MEMORY)
# Needed else both binaries invoke the custom command to generate ${GLUE_LIB_C}
add_dependencies(glue_lib_so_shadow_hfiemu2_asan glue_lib_so_shadow_asan)
target_compile_definitions(glue_lib_so_shadow_hfiemu2_asan PUBLIC WASM_USE_HFI
                                                           PUBLIC HFI_EMULATION2)

##############

include(CTest)
include(Catch)

find_package(Threads REQUIRED)

# Tests executables ###################

add_executable(test_rlbox_glue test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue.cpp)
target_include_directories(test_rlbox_glue PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           )
target_link_libraries(test_rlbox_glue Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
)

target_compile_definitions(test_rlbox_glue PUBLIC GLUE_LIB_WASM2C_PATH="$<TARGET_FILE:glue_lib_so>"
                                           PUBLIC WASM_USE_HFI)

add_dependencies(test_rlbox_glue glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue rt)
endif()
catch_discover_tests(test_rlbox_glue)

####

add_executable(test_rlbox_glue_static test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_static.cpp)
target_include_directories(test_rlbox_glue_static PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_static Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static
)

target_compile_definitions(test_rlbox_glue_static PUBLIC WASM_USE_HFI)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_static rt)
endif()
catch_discover_tests(test_rlbox_glue_static)

####

add_executable(test_rlbox_glue_static_hfiemu test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_static.cpp)
target_include_directories(test_rlbox_glue_static_hfiemu PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_static_hfiemu Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static_hfiemu
)

target_compile_definitions(test_rlbox_glue_static_hfiemu PUBLIC WASM_USE_HFI
                                                         PUBLIC HFI_EMULATION)


add_dependencies(test_rlbox_glue_static_hfiemu glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_static_hfiemu rt)
endif()
catch_discover_tests(test_rlbox_glue_static_hfiemu)

####

add_executable(test_rlbox_glue_static_hfiemu2 test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_static.cpp)
target_include_directories(test_rlbox_glue_static_hfiemu2 PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_static_hfiemu2 Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static_hfiemu2
)

target_compile_definitions(test_rlbox_glue_static_hfiemu2 PUBLIC WASM_USE_HFI
                                                         PUBLIC HFI_EMULATION2)


add_dependencies(test_rlbox_glue_static_hfiemu2 glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_static_hfiemu2 rt)
endif()
catch_discover_tests(test_rlbox_glue_static_hfiemu2)
####

add_executable(test_rlbox_glue_static_hfiemu3 test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_static.cpp)
target_include_directories(test_rlbox_glue_static_hfiemu3 PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_static_hfiemu3 Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static_hfiemu3
)

target_compile_definitions(test_rlbox_glue_static_hfiemu3 PUBLIC WASM_USE_HFI
                                                         PUBLIC HFI_EMULATION3)


add_dependencies(test_rlbox_glue_static_hfiemu3 glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_static_hfiemu3 rt)
endif()
catch_discover_tests(test_rlbox_glue_static_hfiemu3)

####

add_executable(test_rlbox_glue_static_fsgs test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_static.cpp)
target_include_directories(test_rlbox_glue_static_fsgs PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_static_fsgs Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static_fsgs
)

target_compile_definitions(test_rlbox_glue_static_fsgs PUBLIC WASM_USE_SEGMENT)


add_dependencies(test_rlbox_glue_static_fsgs glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_static_fsgs rt)
endif()
catch_discover_tests(test_rlbox_glue_static_fsgs)

####

add_executable(test_rlbox_glue_smallheap test/test_wasm2c_sandbox_glue_main.cpp
                               test/test_wasm2c_sandbox_glue_smallheap.cpp)
target_include_directories(test_rlbox_glue_smallheap PUBLIC ${CMAKE_SOURCE_DIR}/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                           PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                           PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                           PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                           PUBLIC ${GLUE_LIB_WASM_DIR}
                                           )
target_link_libraries(test_rlbox_glue_smallheap Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
                                      glue_lib_static
)

target_compile_definitions(test_rlbox_glue_smallheap PUBLIC WASM_USE_HFI)

add_dependencies(test_rlbox_glue_smallheap glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_smallheap rt)
endif()
catch_discover_tests(test_rlbox_glue_smallheap)

####

add_executable(test_rlbox_glue_embed test/test_wasm2c_sandbox_glue_main.cpp
                                     test/test_wasm2c_sandbox_glue_embedder_vars.cpp)
target_include_directories(test_rlbox_glue_embed PUBLIC ${CMAKE_SOURCE_DIR}/include
                                                 PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                                 PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                                 PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                                 PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                                 PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                 PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                 PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                                                 )
target_link_libraries(test_rlbox_glue_embed Catch2::Catch2
                                      ${CMAKE_THREAD_LIBS_INIT}
                                      ${CMAKE_DL_LIBS}
)

target_compile_definitions(test_rlbox_glue_embed PUBLIC GLUE_LIB_WASM2C_PATH="$<TARGET_FILE:glue_lib_so>"
                                                 PUBLIC WASM_USE_HFI)

add_dependencies(test_rlbox_glue_embed glue_lib_so)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_embed rt)
endif()
catch_discover_tests(test_rlbox_glue_embed)

####

add_executable(test_rlbox_glue_shadow_asan test/test_wasm2c_sandbox_glue_main.cpp
                                           test/test_wasm2c_sandbox_glue_shadow.cpp)
target_include_directories(test_rlbox_glue_shadow_asan PUBLIC ${CMAKE_SOURCE_DIR}/include
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                                       PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                                       PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                       PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                       PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                          )
target_link_libraries(test_rlbox_glue_shadow_asan Catch2::Catch2
                                                  ${CMAKE_THREAD_LIBS_INIT}
                                                  ${CMAKE_DL_LIBS}
)

target_compile_definitions(test_rlbox_glue_shadow_asan PUBLIC GLUE_LIB_WASM2C_PATH="$<TARGET_FILE:glue_lib_so_shadow_asan>"
                                                       PUBLIC WASM_USE_HFI)

add_dependencies(test_rlbox_glue_shadow_asan glue_lib_so_shadow_asan)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_shadow_asan rt)
endif()
# catch_discover_tests(test_rlbox_glue_shadow_asan)

####

add_executable(test_rlbox_glue_shadow_hfiemu2_asan test/test_wasm2c_sandbox_glue_main.cpp
                                           test/test_wasm2c_sandbox_glue_shadow.cpp)
target_include_directories(test_rlbox_glue_shadow_hfiemu2_asan PUBLIC ${CMAKE_SOURCE_DIR}/include
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/include
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue
                                                       PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib
                                                       PUBLIC ${rlbox_SOURCE_DIR}/wasm
                                                       PUBLIC ${mod_wasm2c_SOURCE_DIR}/wasm2c
                                                       PUBLIC ${mod_wasm2c_SOURCE_DIR}/third_party/uvwasi/include
                                                       PUBLIC ${CMAKE_SOURCE_DIR}/../hw_isol_gem5/tests/test-progs/hfi
                          )
target_link_libraries(test_rlbox_glue_shadow_hfiemu2_asan Catch2::Catch2
                                                  ${CMAKE_THREAD_LIBS_INIT}
                                                  ${CMAKE_DL_LIBS}
)

target_compile_definitions(test_rlbox_glue_shadow_hfiemu2_asan PUBLIC GLUE_LIB_WASM2C_PATH="$<TARGET_FILE:glue_lib_so_shadow_hfiemu2_asan>"
                                                       PUBLIC WASM_USE_HFI
                                                       PUBLIC HFI_EMULATION2)

add_dependencies(test_rlbox_glue_shadow_hfiemu2_asan glue_lib_so_shadow_hfiemu2_asan)

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue_shadow_hfiemu2_asan rt)
endif()
# catch_discover_tests(test_rlbox_glue_shadow_hfiemu2_asan)

# Shortcuts ###################

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)
add_dependencies(check test_rlbox_glue)
add_dependencies(check test_rlbox_glue_static_hfiemu)
add_dependencies(check test_rlbox_glue_static_hfiemu2)
add_dependencies(check test_rlbox_glue_static_hfiemu3)
add_dependencies(check test_rlbox_glue_static_fsgs)
add_dependencies(check test_rlbox_glue_static)
add_dependencies(check test_rlbox_glue_smallheap)
add_dependencies(check test_rlbox_glue_embed)
add_dependencies(check test_rlbox_glue_shadow_asan)
add_dependencies(check test_rlbox_glue_shadow_hfiemu2_asan)
add_dependencies(check glue_lib_so)
add_dependencies(check glue_lib_so_shadow_asan)
