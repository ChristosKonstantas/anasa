add_executable(main main.cpp)
target_link_libraries(main PRIVATE engine)

if(OPENVPI)
    target_compile_definitions(main PRIVATE ANASA_OPENVPI)
    target_link_libraries(main PRIVATE openvpi-renderer)

    if(WIN32)
        file(GLOB ONNXRUNTIME_DLLS CONFIGURE_DEPENDS "${ONNXRUNTIME_ROOT}/lib/*.dll")

        if(NOT ONNXRUNTIME_DLLS)
            message(FATAL_ERROR "No ONNX Runtime DLLs found under ONNXRUNTIME_ROOT/lib")
        endif()

        add_custom_command(TARGET main POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ONNXRUNTIME_DLLS}
                "$<TARGET_FILE_DIR:main>"
            VERBATIM
        )
    endif()
endif()