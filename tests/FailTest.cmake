if(WIN32)
    set(MRDOX_FT_RUNNER ${CMAKE_CURRENT_LIST_DIR}/fail_test.bat)
else()
    set(MRDOX_FT_RUNNER ${CMAKE_CURRENT_LIST_DIR}/fail_test.sh)
endif()


function(add_fail_test)
    cmake_parse_arguments(
            FT
            "COMMAND_EXPAND_LISTS"
            "NAME;WORKING_DIRECTORY"
            "COMMAND;CONFIGURATIONS"
            ${ARGV})
    
    if (FT_NAME)
        set(TEST_ARGS NAME ${FT_NAME})
        if (FT_COMMAND)
            list(APPEND TEST_ARGS COMMAND ${MRDOX_FT_RUNNER} ${FT_COMMAND})
        endif()
        if (FT_CONFIGURATIONS)
            list(APPEND TEST_ARGS CONFIGURATIONS ${FT_CONFIGURATIONS})
        endif()
        if (FT_WORKING_DIRECTORY)
            list(APPEND TEST_ARGS WORKING_DIRECTORY ${FT_WORKING_DIRECTORY})
        endif()

        if (FT_COMMAND_EXPAND_LISTS)
            list(APPEND TEST_ARGS COMMAND_EXPAND_LISTS)
        endif()
        add_test(${TEST_ARGS})
    else()
        list(GET ARGV 0 FT_NAME)
        list(POP_FRONT ARGV)
        add_test(${FT_NAME} ${MRDOX_FT_RUNNER} ${ARGV})
    endif()
endfunction()