cmake_minimum_required(VERSION 3.16)
project(VesperaApps C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# ────────────────────────────────────────────────────────────────
# Sysroot — same resolution order as VesperaOS:
#   1. CMake cache variable (-DVESPERA_SYSROOT=…)
#   2. Environment variable  (VESPERA_SYSROOT=…)
#   3. Sibling directory     (../VesperaSysroot, works in VesperaWorkspace layout)
# ────────────────────────────────────────────────────────────────
if (NOT DEFINED VESPERA_SYSROOT)
    if (DEFINED ENV{VESPERA_SYSROOT})
        set(VESPERA_SYSROOT "$ENV{VESPERA_SYSROOT}")
    else ()
        set(VESPERA_SYSROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VesperaSysroot")
    endif ()
endif ()
get_filename_component(VESPERA_SYSROOT "${VESPERA_SYSROOT}" ABSOLUTE)
message(STATUS "VesperaApps – sysroot: ${VESPERA_SYSROOT}")

# ────────────────────────────────────────────────────────────────
# Compiler / Linker flags
# ────────────────────────────────────────────────────────────────
set(COMMON_C_FLAGS
        "-target x86_64-unknown-linux-gnu -nostdlib -nostdinc \
     -fno-stack-protector -fPIE -g -O0 -Wall \
     --sysroot=${VESPERA_SYSROOT} \
     -isystem ${VESPERA_SYSROOT}/usr/include"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

set(COMMON_CXX_FLAGS
        "-target x86_64-unknown-linux-gnu -nostdlib -nostdinc -nostdinc++ \
     -fno-stack-protector -fPIE -g -O0 -Wall \
     -fno-exceptions -fno-rtti \
     --sysroot=${VESPERA_SYSROOT} \
     -isystem ${VESPERA_SYSROOT}/usr/include \
     -isystem ${VESPERA_SYSROOT}/usr/include/c++"
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS}")

set(USER_LD_FLAGS
        "-nostdlib -target x86_64-unknown-linux-gnu -fno-stack-protector -pie \
     --sysroot=${VESPERA_SYSROOT} \
     -L${VESPERA_SYSROOT}/usr/lib"
)

set_source_files_properties(${VESPERA_SYSROOT}/usr/lib/crt0.o PROPERTIES GENERATED TRUE)

# ────────────────────────────────────────────────────────────────
# vespera_bin(<name> [READLINE] [STELLA] [LUA]
#             [SOURCES file…] [EXTRA_LIBS lib…])
#
# Registers an ELF target. Single-source apps that follow the
# bin/<name>.c convention are auto-discovered; use this only for
# multi-source, optional-feature, or aliased targets.
# ────────────────────────────────────────────────────────────────
function(vespera_bin name)
    cmake_parse_arguments(ARG "READLINE;STELLA;LUA" "" "SOURCES;EXTRA_LIBS" ${ARGN})

    if (ARG_SOURCES)
        set(sources ${ARG_SOURCES})
    else ()
        set(sources ${CMAKE_CURRENT_SOURCE_DIR}/bin/${name}.c)
    endif ()

    set(_crt0 ${VESPERA_SYSROOT}/usr/lib/crt0.o)
    add_executable(${name} ${_crt0} ${sources})

    set(IS_CPP FALSE)
    foreach (src ${sources})
        if (src MATCHES "\\.(cpp|cc|cxx)$")
            set(IS_CPP TRUE)
        endif ()
    endforeach ()

    set(_link_libs "")

    if (ARG_EXTRA_LIBS)
        list(APPEND _link_libs ${ARG_EXTRA_LIBS})
    endif ()

    if (ARG_READLINE)
        list(APPEND _link_libs ${VESPERA_SYSROOT}/usr/lib/libreadline.a)
    endif ()

    if (ARG_STELLA)
        list(APPEND _link_libs
                ${VESPERA_SYSROOT}/usr/lib/libstella.a
                ${VESPERA_SYSROOT}/usr/lib/liblvgl.a
        )
        target_include_directories(${name} PRIVATE
                ${VESPERA_SYSROOT}/usr/include/lvgl
        )
    endif ()

    if (ARG_LUA)
        list(APPEND _link_libs
                ${VESPERA_SYSROOT}/usr/lib/liblua.a
                ${VESPERA_SYSROOT}/usr/lib/liblvespera.a
        )
        target_include_directories(${name} PRIVATE
                ${VESPERA_SYSROOT}/usr/include/lua
        )
    endif ()

    if (IS_CPP OR ARG_STELLA)
        list(APPEND _link_libs ${VESPERA_SYSROOT}/usr/lib/libvesp++.a)
        set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)
        target_include_directories(${name} PRIVATE ${VESPERA_SYSROOT}/usr/include/c++)
    endif ()

    list(APPEND _link_libs ${VESPERA_SYSROOT}/usr/lib/libvesp.a)

    target_link_libraries(${name} PRIVATE ${_link_libs})

    set_target_properties(${name} PROPERTIES
            LINK_FLAGS "${USER_LD_FLAGS}"
    )

    set_property(GLOBAL APPEND PROPERTY VESPERA_BIN_TARGETS ${name})
endfunction()

# ────────────────────────────────────────────────────────────────
# Explicit registrations
# Required for: multi-source, READLINE/STELLA/LUA, aliased sources.
# Single-source bin/<name>.c targets are auto-discovered below.
# ────────────────────────────────────────────────────────────────
vespera_bin(nox
        READLINE
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/nox/nox.c
)

vespera_bin(env SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/env.c)
vespera_bin(printenv SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/env.c)

vespera_bin(crepusculum
        STELLA LUA
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/crepusculum/crepusculum.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/crepusculum/xcursor_loader.c
)

vespera_bin(testapp STELLA SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/testapp.c)
vespera_bin(calculator STELLA SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/calculator.c)

vespera_bin(settings
        STELLA
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/settings.cpp
)

vespera_bin(cppapp
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/cppapp.cpp
)

vespera_bin(firmament
        STELLA
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/theme/theme.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/shell/desktop.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/shell/astra.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/shell/topbar.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/shell/taskbar.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/widgets/clock_widget.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/widgets/battery_widget.c
        # ${CMAKE_CURRENT_SOURCE_DIR}/bin/firmament/widgets/sysinfo_widget.c
)

# ────────────────────────────────────────────────────────────────
# Auto-discovery
# Every bin/<name>.c that is NOT in the explicit list becomes a
# vespera_bin(<name>) automatically — just drop the .c file.
# C++-only targets (*.cpp) must always be registered explicitly.
# ────────────────────────────────────────────────────────────────
set(_EXPLICIT_BINS
        nox env printenv crepusculum firmament
        testapp calculator settings cppapp
)

file(GLOB _auto_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/*.c
)

foreach (_src ${_auto_sources})
    get_filename_component(_name ${_src} NAME_WE)
    if (NOT _name IN_LIST _EXPLICIT_BINS)
        vespera_bin(${_name})
    endif ()
endforeach ()

get_property(_all_bins GLOBAL PROPERTY VESPERA_BIN_TARGETS)

install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/
        DESTINATION ${VESPERA_SYSROOT}/usr/share/icons
        COMPONENT apps
)

install(
        TARGETS ${_all_bins}
        RUNTIME
        DESTINATION ${VESPERA_SYSROOT}/bin
        COMPONENT apps
)

add_custom_target(install_apps
        DEPENDS ${_all_bins}
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component apps
        COMMENT "Installing apps to sysroot"
)