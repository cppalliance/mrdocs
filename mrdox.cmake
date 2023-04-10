
macro(_mrdox_prep_vars)
    if (NOT DEFINED MRDOX_EXECUTABLE)
        set(MRDOX_EXECUTABLE $<TARGET_FILE:mrdox>)
        set(MRDOX_EXECUTABLE_DEPENDENCY mrdox)
    endif()

    if (NOT DEFINED MRDOX_COMPILE_COMMANDS)
        if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
            message(FATAL_ERROR "MrDox build script requires either CMAKE_EXPORT_COMPILE_COMMANDS=ON or MRDOX_COMPILE_COMMANDS to be set")
        else()
            set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        endif()
        set(MRDOX_COMPILE_COMMANDS ${CMAKE_BINARY_DIR}/compile_commands.json)
    endif()
endmacro()


function(mrdox target config)
    _mrdox_prep_vars()

    get_filename_component(FORMAT ${target} EXT)
    string(SUBSTRING ${FORMAT} 1 8 FORMAT)
    add_custom_command(
        OUTPUT ${target}
        COMMAND mrdox --config=${CMAKE_CURRENT_SOURCE_DIR}/${config}
            -p ${MRDOX_COMPILE_COMMANDS} ${ARGN}
            --format=${FORMAT}
            --output=${CMAKE_CURRENT_BINARY_DIR}
            --filename=${target}
        MAIN_DEPENDENCY ${config} # scanner!
        DEPENDS ${MRDOX_EXECUTABLE_DEPENDENCY} ${ARGN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()