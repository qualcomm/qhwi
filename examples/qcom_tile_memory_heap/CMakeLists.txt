add_qhwi_example(qhwi_example_qcom_tile_memory_heap)
add_subdirectory(data/shaders)

if (ANDROID)
target_compile_definitions(qhwi_example_qcom_tile_memory_heap PRIVATE
    QHWI_DATA_DIR="/sdcard/Android/data/com.qualcomm.qhwi_example_qcom_tile_memory_heap/files/data"
)
endif()

add_dependencies(qhwi_example_qcom_tile_memory_heap qhwi_example_qcom_tile_memory_heap_shaders)
