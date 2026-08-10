function(anasa_add_module TARGET_NAME)

    set(options HEADER_ONLY)
    set(oneValueArgs FOLDER)
    set(multiValueArgs SOURCES)

    cmake_parse_arguments(
        MODULE
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "anasa_add_module(${TARGET_NAME}): no SOURCES provided")
    endif()

    if(MODULE_HEADER_ONLY)

        add_library(${TARGET_NAME} INTERFACE ${MODULE_SOURCES})

        target_include_directories(${TARGET_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/src)

    else()

        add_library(${TARGET_NAME} STATIC ${MODULE_SOURCES})

        target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/src)

    endif()

    if(MODULE_FOLDER)
        set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "${MODULE_FOLDER}")
    endif()

    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX ""
        FILES ${MODULE_SOURCES}
    )

endfunction()