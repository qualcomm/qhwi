add_qhwi_example(qhwi_example_gpl)
add_subdirectory(data/shaders)

target_compile_definitions(qhwi_example_gpl PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
# Commenting the dependency on the shaders to avoid recompiling the 1024+ combinations of shaders unless 
# explicitly needed
#add_dependencies(qhwi_example_gpl qhwi_example_gpl_shaders)