add_qhwi_example(qhwi_example_qcom_image_proc)
add_subdirectory(data/shaders)

target_compile_definitions(qhwi_example_qcom_image_proc PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

add_dependencies(qhwi_example_qcom_image_proc qhwi_example_qcom_image_proc_shaders)
