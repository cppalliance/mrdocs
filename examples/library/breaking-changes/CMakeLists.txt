add_executable(mrdocs-breaking-changes-example
    src/main.cpp
    src/Corpus.cpp
    src/Diff.cpp
    src/BreakingChangesGenerator.cpp
)
target_link_libraries(mrdocs-breaking-changes-example PRIVATE mrdocs-core)
target_compile_features(mrdocs-breaking-changes-example PRIVATE cxx_std_23)
if (MRDOCS_CLANG)
    target_compile_options(mrdocs-breaking-changes-example PRIVATE -Wno-covered-switch-default)
endif()

if (BUILD_TESTING)
    add_test(
        NAME mrdocs-library-example-breaking-changes
        COMMAND ${CMAKE_COMMAND}
                "-DTOOL=$<TARGET_FILE:mrdocs-breaking-changes-example>"
                "-DV1_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test/v1/mrdocs.yml"
                "-DV2_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/test/v2/mrdocs.yml"
                "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/test/expected.txt"
                "-DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/actual.txt"
                "-DMRDOCS_ROOT=${PROJECT_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/test/run.cmake"
    )
endif()
