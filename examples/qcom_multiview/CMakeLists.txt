add_qhwi_example(qhwi_example_qcom_multiview)
add_subdirectory(data/shaders)

target_compile_definitions(qhwi_example_qcom_multiview PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

add_dependencies(qhwi_example_qcom_multiview qhwi_example_qcom_multiview_shaders)
