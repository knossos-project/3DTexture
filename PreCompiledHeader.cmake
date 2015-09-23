#generates a precompiled header out of the the file specified
#and includes it in every file in the project
#this can be disabled  with option »USE_PRECOMPILED_HEADER«
option(USE_PRECOMPILED_HEADER "Use precompiled headers with gcc/clang" ON)
macro(add_precompiled_header _input)
    if(NOT USE_PRECOMPILED_HEADER)
        message(STATUS "precompiled headers disabled")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        message(STATUS "using precompiled header ${_input}")
        #generate include-dir switches
        get_target_property(includes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
        foreach(dir ${includes})
            list(APPEND incs "-I${dir}")
        endforeach()
        #generate list of compiler flags for current build type
        #using strings instead of lists wrecks up the commandline
        string(REPLACE " " ";" target_flags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
        #command to generate PCH
        get_directory_property(DirDefs DIRECTORY ${CMAKE_SOURCE_DIR} COMPILE_DEFINITIONS)
        foreach(def ${DirDefs})
            list(APPEND definitions "-D${def}")
        endforeach()
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${_input}.gch
            COMMAND ${CMAKE_CXX_COMPILER} -x c++-header ${target_flags} ${definitions} ${incs} ${CMAKE_CURRENT_SOURCE_DIR}/${_input}
            MAIN_DEPENDENCY ${_input})
        #create PCH target
        add_custom_target(${PROJECT_NAME}_gch ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_input}.gch VERBATIM)
        #include PCH in every src-file
        set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-include \"${CMAKE_CURRENT_SOURCE_DIR}/${_input}\" -Winvalid-pch")
        #delete PCH on clean
        set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${_input}.gch")
        #make project target depend on PCH target
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_gch)
    else()
        message(STATUS "precompiled headers unsupported")
    endif()
endmacro()
