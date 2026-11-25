set(ANASA_SANITIZER "none" CACHE STRING "Runtime checks: none, address-undefined, or thread")
set_property(CACHE ANASA_SANITIZER PROPERTY STRINGS none address-undefined thread)

if(ANASA_SANITIZER STREQUAL "none")
    return()
elseif(ANASA_SANITIZER STREQUAL "address-undefined")
    set(anasaSanitizerFlag "-fsanitize=address,undefined")
elseif(ANASA_SANITIZER STREQUAL "thread")
    set(anasaSanitizerFlag "-fsanitize=thread")
else()
    message(FATAL_ERROR "Unknown ANASA_SANITIZER: ${ANASA_SANITIZER}")
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR "ANASA sanitizer builds currently require Linux with GCC or Clang. On Windows, use WSL.")
endif()

# Apply before add_subdirectory(): instrument project libraries and executables.
add_compile_options(
    ${anasaSanitizerFlag}
    -O1
    -g
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
)

# The final executables must link the matching sanitizer runtime.
add_link_options(${anasaSanitizerFlag})

if(ANASA_SANITIZER STREQUAL "address-undefined")
    add_compile_options(-fno-sanitize-recover=undefined)
endif()

message(STATUS "ANASA sanitizer: ${ANASA_SANITIZER}")