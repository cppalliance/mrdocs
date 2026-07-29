if(NOT TOOL OR NOT V1_CONFIG OR NOT V2_CONFIG OR NOT EXPECTED OR NOT ACTUAL)
    message(FATAL_ERROR "run.cmake: missing required variable")
endif()

# The example resolves shared assets from <mrdocs-root>/share/mrdocs (the
# installed layout). In the source tree those assets live under data/, so
# stage a root whose share/ mirrors data/ and point the tool at it. This lets
# the example keep demonstrating the default install-layout resolution.
if(DATA_DIR AND STAGE_ROOT)
    file(REMOVE_RECURSE "${STAGE_ROOT}/share")
    file(COPY "${DATA_DIR}/" DESTINATION "${STAGE_ROOT}/share")
    set(ENV{MRDOCS_ROOT} "${STAGE_ROOT}")
else()
    set(ENV{MRDOCS_ROOT} "${MRDOCS_ROOT}")
endif()

execute_process(
    COMMAND "${TOOL}" "${V1_CONFIG}" "${V2_CONFIG}"
    OUTPUT_FILE "${ACTUAL}"
    RESULT_VARIABLE _rc
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Tool exited with status ${_rc}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${ACTUAL}" "${EXPECTED}"
    RESULT_VARIABLE _diff
)

if(NOT _diff EQUAL 0)
    message(FATAL_ERROR
        "Output differs from expected\n"
        "  actual:   ${ACTUAL}\n"
        "  expected: ${EXPECTED}\n"
        "Update the golden if the new output is intentional."
    )
endif()
