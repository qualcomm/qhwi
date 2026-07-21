# TODO: remove test_util

set(QHWI_EXAMPLES_COMMON_SOURCE_FILES src/qhwi_example.cpp src/qhwi_example.h scripts/shaderUtil.py)

add_library(qhwi_examples_common ${QHWI_EXAMPLES_COMMON_SOURCE_FILES})

target_include_directories(qhwi_examples_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CXXOPTS_INCLUDE_DIRS})
target_link_libraries(qhwi_examples_common PUBLIC qhwi)
target_compile_features(qhwi_examples_common PUBLIC cxx_std_20)