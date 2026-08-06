include(CTest)

DownloadFile(cxxopts https://raw.githubusercontent.com/jarro2783/cxxopts/master/include/cxxopts.hpp)
set(CXXOPTS_INCLUDE_DIRS ${cxxopts_SOURCE_DIR})

file(GLOB_RECURSE QHWI_TEST_SOURCE_FILES src/*.cpp src/*.h util/*)
file(GLOB_RECURSE QHWI_ANDROID_UTIL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../util/qhwi_android_util.*)

if(ANDROID)
    DownloadExtractFile(vk_vl https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/vulkan-sdk-1.3.268.0/android-binaries-vulkan-sdk-1.3.268.0-android.tar.gz)
    set(VK_VALIDATION_LAYER_PREBUILT_LIBRARY_DIR ${vk_vl_SOURCE_DIR})

    add_library(qhwi_test SHARED ${QHWI_TEST_SOURCE_FILES} ${QHWI_ANDROID_UTIL_SOURCE_FILES})
    add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    target_include_directories(qhwi_test PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue ${CMAKE_CURRENT_SOURCE_DIR}/../util)
    target_link_libraries(qhwi_test PRIVATE
            android native_app_glue log "-u ANativeActivity_onCreate")
    set(QHWI_TEST_FILES_DIR /sdcard/Android/data/com.qualcomm.qhwi_test/files)
    set(ANDROID_ABI arm64-v8a)
    file(COPY ${VK_VALIDATION_LAYER_PREBUILT_LIBRARY_DIR}/${ANDROID_ABI}/libVkLayer_khronos_validation.so
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/jniLibs/${ANDROID_ABI})
else()
    add_executable(qhwi_test
        ${QHWI_TEST_SOURCE_FILES})
endif()

target_include_directories(qhwi_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CXXOPTS_INCLUDE_DIRS}
)
target_link_libraries(qhwi_test PRIVATE
    qhwi
)

qre_set_target_recommended_warnings(qhwi_test)

if (QHWI_ENABLE_PIL)
    target_compile_definitions(qhwi_test PRIVATE QHWI_ENABLE_PIL)
endif()

target_compile_features(qhwi_test PUBLIC cxx_std_20)
if(ANDROID)
    target_compile_definitions(qhwi_test PRIVATE
        QHWI_DATA_DIR="${QHWI_TEST_FILES_DIR}/data"
        QHWI_TEST_REF_DATA_DIR="${QHWI_TEST_FILES_DIR}/ref/data"
    )
else()
    target_compile_definitions(qhwi_test PRIVATE
        QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
        QHWI_TEST_REF_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ref/data"
    )
endif()

if (QHWI_ENABLE_D3D12_BACKEND)
add_custom_command(TARGET qhwi_test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_BIN_DIR}/D3D12Core.dll" "$<TARGET_FILE_DIR:qhwi_test>/D3D12/D3D12Core.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_BIN_DIR}/d3d12SDKLayers.dll" "$<TARGET_FILE_DIR:qhwi_test>/D3D12/d3d12SDKLayers.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_PIX_BIN_DIR}/WinPixEventRuntime.dll" "$<TARGET_FILE_DIR:qhwi_test>/WinPixEventRuntime.dll")
endif()

if (QHWI_ENABLE_D3D12_BACKEND AND QHWI_ENABLE_COMPRESSED_STORAGE)
add_custom_command(TARGET qhwi_test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_DSTORAGE_BIN_DIR}/dstorage.dll" "$<TARGET_FILE_DIR:qhwi_test>/dstorage.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_DSTORAGE_BIN_DIR}/dstoragecore.dll" "$<TARGET_FILE_DIR:qhwi_test>/dstoragecore.dll")
endif()

add_subdirectory(data/shaders)

add_dependencies(qhwi_test qhwi_test_shaders qhwi_shaders)

function(add_qhwi_test_fn name backend)
    add_test(NAME ${name}_${backend} COMMAND qhwi_test --tests=${name} --backends=${backend} --onscreen=false)
    set_property(TEST ${name}_${backend} PROPERTY SKIP_RETURN_CODE 2)
endfunction()

function(add_qhwi_test name)
    if (QHWI_ENABLE_D3D12_BACKEND)
        add_qhwi_test_fn(${name} d3d12)
    endif()
    if (QHWI_ENABLE_VULKAN_BACKEND)
        add_qhwi_test_fn(${name} vk)
    endif()
endfunction()

# Generate a C++ header from the test names declared in tests.cmake so
# test_app.cpp does not need its own duplicate list.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests.cmake)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests.cmake QHWI_TESTS_CMAKE_CONTENT)
string(REGEX MATCHALL "add_qhwi_test\\([A-Za-z0-9_]+\\)" QHWI_TEST_INVOCATIONS "${QHWI_TESTS_CMAKE_CONTENT}")
set(QHWI_TEST_LIST_INC_CONTENT "// Generated from tests.cmake by CMake. Do not edit directly.\n")
foreach(invocation ${QHWI_TEST_INVOCATIONS})
    string(REGEX REPLACE "add_qhwi_test\\(([A-Za-z0-9_]+)\\)" "\\1" test_name "${invocation}")
    string(APPEND QHWI_TEST_LIST_INC_CONTENT "ADD_QHWI_TEST(${test_name})\n")
endforeach()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/qhwi_test_list.inc "${QHWI_TEST_LIST_INC_CONTENT}")
target_include_directories(qhwi_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

include(tests.cmake)
