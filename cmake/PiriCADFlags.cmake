# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compile flags. kentoscad.md §7.3 — these are not preferences.
#
#   -ffast-math / /fp:fast are BANNED: they break NaN checks, reorder addition and
#   destroy robust predicate correctness.
#
#   -ffp-contract=off is MANDATORY: FMA does not round the intermediate result, so
#   the same source gives different answers on x86 and Apple Silicon. Software that
#   produces official survey documents must return bit-identical areas,
#   intersections and adjustments on every platform.
#
# Enforced by scripts/ci-gate-fp-flags.sh.

add_library(kentos_flags INTERFACE)

if(MSVC)
    target_compile_options(kentos_flags INTERFACE
        /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8
        /W4
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/fp:precise>
    )
else()
    target_compile_options(kentos_flags INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wcast-align -Wunused
        -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wdouble-promotion -Wformat=2
        -fno-fast-math
        -ffp-contract=off
        -fvisibility=hidden
        $<$<CONFIG:Release>:-O2>
    )
endif()

# A guard rather than a comment: a flag that silently changes numerical results
# must not be introducible by accident.
foreach(_flag IN ITEMS CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
    if("${${_flag}}" MATCHES "fast-math|fp:fast|ffp-contract=fast")
        message(FATAL_ERROR
            "${_flag} contains a fast-math style flag. Banned by kentoscad.md §7.3: "
            "it breaks cross-platform bit-identical results, which are a legal "
            "requirement for official survey output.")
    endif()
endforeach()
