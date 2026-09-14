include(FetchContent)

# Tracktion Engine v3.2.0 pins this JUCE revision. Keep the pair together until
# a dedicated dependency upgrade proves a newer combination.
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/19edd538429c93d277bf95b55aaa7e3eb545f951.tar.gz
    URL_HASH SHA256=5be3406ca3c7e4e757b1ca4d8e9083024563ce05a3a675597adadbcc8f334e82
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(JUCE)

# SOURCE_SUBDIR avoids Tracktion's top-level example build and its SSH JUCE
# submodule URL. The application supplies the exact JUCE target above.
FetchContent_Declare(tracktion_engine
    URL https://github.com/Tracktion/tracktion_engine/archive/0a5f4e6a5f53d09c89b414a44386a12df7fa1ec6.tar.gz
    URL_HASH SHA256=06536e8d5d68a22100bab0c8db59402638eded4b7c7decce19d71b511d587238
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR modules)

FetchContent_MakeAvailable(tracktion_engine)

# c2pa-cpp v0.26.9 backed by c2pa-rs 0.90.15. The non-existent SOURCE_SUBDIR
# populates each immutable dependency without enabling its upstream examples and
# network-dependent test suite in this application build.
FetchContent_Declare(c2pa_cpp_source
    GIT_REPOSITORY https://github.com/contentauth/c2pa-cpp.git
    GIT_TAG 26f7c8cd3bdb421d0df636fbcd0cbdf2cba25447
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR c2paseq-no-upstream-build)
FetchContent_MakeAvailable(c2pa_cpp_source)

if(NOT APPLE)
    message(FATAL_ERROR "PR 006 c2pa-cpp integration currently supports macOS only")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(C2PASEQ_C2PA_RUNTIME_ARCHIVE c2pa-v0.90.15-aarch64-apple-darwin.zip)
    set(C2PASEQ_C2PA_RUNTIME_HASH 119c2cb2369d5265b016a813d35199defcf01711816bb1b0103f8d583f0c3dba)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(C2PASEQ_C2PA_RUNTIME_ARCHIVE c2pa-v0.90.15-x86_64-apple-darwin.zip)
    set(C2PASEQ_C2PA_RUNTIME_HASH c0f064e35178828eb21c757c1a499d82e79cced84673d00125c490169615830b)
else()
    message(FATAL_ERROR "No pinned c2pa-rs macOS runtime is configured for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

FetchContent_Declare(c2pa_prebuilt
    URL https://github.com/contentauth/c2pa-rs/releases/download/c2pa-v0.90.15/${C2PASEQ_C2PA_RUNTIME_ARCHIVE}
    URL_HASH SHA256=${C2PASEQ_C2PA_RUNTIME_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR c2paseq-no-upstream-build)
FetchContent_MakeAvailable(c2pa_prebuilt)

set(C2PASEQ_C2PA_C_LIBRARY_SOURCE "${c2pa_prebuilt_SOURCE_DIR}/lib/libc2pa_c.dylib")
if(NOT EXISTS "${C2PASEQ_C2PA_C_LIBRARY_SOURCE}")
    message(FATAL_ERROR "Pinned c2pa-rs runtime was not populated")
endif()

set(C2PASEQ_C2PA_RUNTIME_DIR "${CMAKE_BINARY_DIR}/c2pa-runtime")
file(MAKE_DIRECTORY "${C2PASEQ_C2PA_RUNTIME_DIR}")
set(C2PASEQ_C2PA_C_LIBRARY "${C2PASEQ_C2PA_RUNTIME_DIR}/libc2pa_c.dylib")
configure_file("${C2PASEQ_C2PA_C_LIBRARY_SOURCE}"
               "${C2PASEQ_C2PA_C_LIBRARY}" COPYONLY)

find_program(C2PASEQ_INSTALL_NAME_TOOL install_name_tool REQUIRED)
find_program(C2PASEQ_CODESIGN codesign REQUIRED)
execute_process(
    COMMAND "${C2PASEQ_INSTALL_NAME_TOOL}" -id @rpath/libc2pa_c.dylib "${C2PASEQ_C2PA_C_LIBRARY}"
    RESULT_VARIABLE C2PASEQ_INSTALL_NAME_RESULT
    OUTPUT_QUIET ERROR_QUIET)
if(NOT C2PASEQ_INSTALL_NAME_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not make the pinned c2pa-rs runtime relocatable")
endif()

add_library(c2pa_c SHARED IMPORTED GLOBAL)
set_target_properties(c2pa_c PROPERTIES
    IMPORTED_LOCATION "${C2PASEQ_C2PA_C_LIBRARY}")

add_library(c2pa_cpp STATIC
    "${c2pa_cpp_source_SOURCE_DIR}/src/c2pa_amalgam.cpp")
target_include_directories(c2pa_cpp PUBLIC
    "${c2pa_cpp_source_SOURCE_DIR}/include"
    "${c2pa_prebuilt_SOURCE_DIR}/include")
target_compile_features(c2pa_cpp PUBLIC cxx_std_20)
target_link_libraries(c2pa_cpp PUBLIC c2pa_c)
target_compile_options(c2pa_cpp PRIVATE -Wno-deprecated-declarations)

function(c2paseq_enable_c2pa target)
    target_link_libraries(${target} PRIVATE c2pa_cpp)
    set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "@loader_path")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${C2PASEQ_C2PA_C_LIBRARY}"
            "$<TARGET_FILE_DIR:${target}>/libc2pa_c.dylib"
        VERBATIM)
    get_target_property(C2PASEQ_TARGET_IS_BUNDLE ${target} MACOSX_BUNDLE)
    if(C2PASEQ_TARGET_IS_BUNDLE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${C2PASEQ_CODESIGN}" --force --sign - --timestamp=none
                "$<TARGET_FILE_DIR:${target}>/libc2pa_c.dylib"
            COMMAND "${C2PASEQ_CODESIGN}" --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()
