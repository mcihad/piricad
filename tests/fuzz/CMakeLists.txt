# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fuzz targets. .claude/io.md R19 and test.md R9: every parser has a
# libFuzzer/AFL++ target here, running continuously.
#
# Off by default and Clang-only, because `-fsanitize=fuzzer` is a Clang feature.
# The seed corpora under tohum/ are NOT dead weight when this is off: every one of
# them is replayed through the same reader by `kentos_tests`
# (tests/unit/test_io.cpp), so the corpus is exercised on every ordinary build.
#
# test.md P8: the FP flags must not differ from the product build, so these link
# kentos_flags exactly like everything else. -ffp-contract=off stays on.

option(KENTOS_BUILD_FUZZ "Build the libFuzzer targets (Clang only)" OFF)

if(NOT KENTOS_BUILD_FUZZ)
    return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "KENTOS_BUILD_FUZZ=ON needs Clang: -fsanitize=fuzzer is a Clang feature.\n"
        "  cmake --preset dev -DKENTOS_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++\n"
        "  Or configure with -DKENTOS_BUILD_FUZZ=OFF; the seed corpora are still\n"
        "  replayed by kentos_tests either way.")
endif()

function(kentos_fuzz_target name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE kentos_core kentos_command kentos_io kentos_flags)

    # Fuzzing without ASan and UBSan finds only the crashes that happen to be
    # fatal; io.md Enforcement asks for both.
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined
                                           -fno-omit-frame-pointer)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

kentos_fuzz_target(kentos_fuzz_proje fuzz_proje.cpp)

# The DXF harness exercises the KentOSCad side of the GDAL seam, so it only exists
# when that seam is compiled in.
if(KENTOS_WITH_GDAL)
    kentos_fuzz_target(kentos_fuzz_dxf fuzz_dxf.cpp)
endif()

# A short deterministic run per target, so a broken harness is caught by `ctest`
# rather than by the nightly fuzzing job three days later.
#
# THE FIRST DIRECTORY IS WHERE libFuzzer WRITES. It saves every new unit it finds
# into that directory, so passing the checked-in seed corpus first would have the
# test suite spraying a few hundred unnamed binaries into the source tree on every
# run. The scratch directory in the build tree goes first and the seeds follow as
# read-only inputs; the corpus stays exactly the eight named files a reviewer can
# reason about, and a genuinely interesting find is copied in deliberately
# (test.md R10).
set(KENTOS_FUZZ_SCRATCH "${CMAKE_BINARY_DIR}/fuzz-corpus")
file(MAKE_DIRECTORY "${KENTOS_FUZZ_SCRATCH}/proje" "${KENTOS_FUZZ_SCRATCH}/dxf")

add_test(NAME fuzz_proje_smoke
    COMMAND kentos_fuzz_proje "${KENTOS_FUZZ_SCRATCH}/proje"
            "${CMAKE_CURRENT_SOURCE_DIR}/tohum/proje"
            -runs=2000 -max_total_time=30 -seed=1)

if(KENTOS_WITH_GDAL)
    add_test(NAME fuzz_dxf_smoke
        COMMAND kentos_fuzz_dxf "${KENTOS_FUZZ_SCRATCH}/dxf"
                "${CMAKE_CURRENT_SOURCE_DIR}/tohum/dxf"
                -runs=2000 -max_total_time=30 -seed=1)
endif()
