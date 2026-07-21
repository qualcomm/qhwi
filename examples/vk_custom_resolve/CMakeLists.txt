add_qhwi_example(qhwi_example_vk_custom_resolve)
add_subdirectory(data/shaders)

target_compile_definitions(qhwi_example_vk_custom_resolve PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

# Link against NVIDIA Aftermath if enabled and found
if(QHWI_AFTERMATH_FOUND)
    target_link_libraries(qhwi_example_vk_custom_resolve PRIVATE AftermathSDK::AftermathSDK)
endif()

add_dependencies(qhwi_example_vk_custom_resolve qhwi_example_vk_custom_resolve_shaders)
