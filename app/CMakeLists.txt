add_executable(velk_ui_app
    main.cpp
)

target_link_libraries(velk_ui_app PRIVATE velk velk_ui velk_render velk_runtime velk_text)

# Copy scene files to output directory
add_custom_command(TARGET velk_ui_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:velk_ui_app>/scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/test/scenes/hello.json"
        "$<TARGET_FILE_DIR:velk_ui_app>/scenes/hello.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/test/scenes/stack_test.json"
        "$<TARGET_FILE_DIR:velk_ui_app>/scenes/stack_test.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/test/scenes/dashboard.json"
        "$<TARGET_FILE_DIR:velk_ui_app>/scenes/dashboard.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/test/assets"
        "$<TARGET_FILE_DIR:velk_ui_app>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:velk_ui_app>/scenes/images"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/test/scenes/images/velk-icon.png"
        "$<TARGET_FILE_DIR:velk_ui_app>/scenes/images/velk-icon.png"
    COMMENT "Copying scene files to output directory"
)
