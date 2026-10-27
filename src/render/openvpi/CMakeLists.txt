set(ONNXRUNTIME_ROOT "" CACHE PATH "Extracted ONNX Runtime CPU SDK")
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime
    PATHS "${ONNXRUNTIME_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)

add_library(anasa-onnxruntime INTERFACE)
target_include_directories(anasa-onnxruntime SYSTEM INTERFACE "${ONNXRUNTIME_INCLUDE_DIR}")
target_link_libraries(anasa-onnxruntime INTERFACE "${ONNXRUNTIME_LIBRARY}")

anasa_add_module(openvpi-renderer
    SOURCES OpenVpiRenderer.hpp OpenVpiRenderer.cpp)
target_link_libraries(openvpi-renderer PUBLIC audio-buffer anasa-onnxruntime)
if(MSVC)
    target_compile_options(openvpi-renderer PRIVATE /W4 /permissive-)
    target_compile_definitions(openvpi-renderer PUBLIC NOMINMAX)
else()
    target_compile_options(openvpi-renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()