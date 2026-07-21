add_qhwi_example(qhwi_example_qcom_tile_shading)

target_compile_definitions(qhwi_example_qcom_tile_shading PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

add_subdirectory(data/shaders)
add_dependencies(qhwi_example_qcom_tile_shading qhwi_example_qcom_tile_shading_shaders)
