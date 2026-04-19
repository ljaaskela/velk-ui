add_executable(velk_ui_fluent
    main.cpp
)

target_link_libraries(velk_ui_fluent PRIVATE velk velk_ui velk_render velk_runtime velk_text)

# Stage scenes + shared assets next to the executable. The fluent sample
# reuses the simple sample's HDRI for now (single asset dir per repo).
add_custom_command(TARGET velk_ui_fluent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:velk_ui_fluent>/scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scenes/fluent.json"
        "$<TARGET_FILE_DIR:velk_ui_fluent>/scenes/fluent.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../simple/assets"
        "$<TARGET_FILE_DIR:velk_ui_fluent>/assets"
    COMMENT "Copying fluent sample scene + assets"
)
