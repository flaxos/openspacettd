set(GS_COMPAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.2.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.3.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.4.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.5.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.6.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.7.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.8.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.9.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.10.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.11.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_12.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_13.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_14.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_15.nut
)

foreach(GS_COMPAT_SOURCE_FILE IN LISTS GS_COMPAT_SOURCE_FILES)
    string(REPLACE "${CMAKE_SOURCE_DIR}/bin/" "" GS_COMPAT_SOURCE_FILE_NAME "${GS_COMPAT_SOURCE_FILE}")
    string(CONCAT GS_COMPAT_BINARY_FILE "${CMAKE_BINARY_DIR}/" "${GS_COMPAT_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${GS_COMPAT_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${GS_COMPAT_SOURCE_FILE}
                    ${GS_COMPAT_BINARY_FILE}
            MAIN_DEPENDENCY ${GS_COMPAT_SOURCE_FILE}
            COMMENT "Copying ${GS_COMPAT_SOURCE_FILE_NAME}"
    )

    list(APPEND GS_COMPAT_BINARY_FILES ${GS_COMPAT_BINARY_FILE})
endforeach()

# Create a new target which copies all compat files
add_custom_target(gs_compat_files
        DEPENDS ${GS_COMPAT_BINARY_FILES}
)

add_dependencies(openttd
    gs_compat_files
)

# Bundle the OpenSpaceTTD UAT/demo GameScript with development builds. The
# script adds persistent Story Book guidance, goals, and map markers to the
# deterministic three-world demo save.
set(OPENSPACETTD_UAT_DEMO_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/openspacettd_uat/info.nut
	${CMAKE_CURRENT_SOURCE_DIR}/openspacettd_uat/main.nut
)

foreach(OPENSPACETTD_UAT_DEMO_SOURCE_FILE IN LISTS OPENSPACETTD_UAT_DEMO_SOURCE_FILES)
    string(REPLACE "${CMAKE_SOURCE_DIR}/bin/" "" OPENSPACETTD_UAT_DEMO_SOURCE_FILE_NAME "${OPENSPACETTD_UAT_DEMO_SOURCE_FILE}")
    string(CONCAT OPENSPACETTD_UAT_DEMO_BINARY_FILE "${CMAKE_BINARY_DIR}/" "${OPENSPACETTD_UAT_DEMO_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${OPENSPACETTD_UAT_DEMO_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${CMAKE_BINARY_DIR}/game/openspacettd_uat
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${OPENSPACETTD_UAT_DEMO_SOURCE_FILE}
                    ${OPENSPACETTD_UAT_DEMO_BINARY_FILE}
            MAIN_DEPENDENCY ${OPENSPACETTD_UAT_DEMO_SOURCE_FILE}
            COMMENT "Copying ${OPENSPACETTD_UAT_DEMO_SOURCE_FILE_NAME}"
    )

    list(APPEND OPENSPACETTD_UAT_DEMO_BINARY_FILES ${OPENSPACETTD_UAT_DEMO_BINARY_FILE})
endforeach()

add_custom_target(openspacettd_uat_demo_files
        DEPENDS ${OPENSPACETTD_UAT_DEMO_BINARY_FILES}
)

add_dependencies(openttd
    openspacettd_uat_demo_files
)

# Optional authored mission; ordinary New Game still selects no GameScript.
set(FIRST_LINE_BINARY_FILES)
foreach(FIRST_LINE_FILE info.nut main.nut)
    set(FIRST_LINE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/openspacettd_first_line/${FIRST_LINE_FILE})
    set(FIRST_LINE_BINARY ${CMAKE_BINARY_DIR}/game/openspacettd_first_line/${FIRST_LINE_FILE})
    add_custom_command(OUTPUT ${FIRST_LINE_BINARY}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/game/openspacettd_first_line
            COMMAND ${CMAKE_COMMAND} -E copy ${FIRST_LINE_SOURCE} ${FIRST_LINE_BINARY}
            MAIN_DEPENDENCY ${FIRST_LINE_SOURCE}
            COMMENT "Copying First Permanent Line ${FIRST_LINE_FILE}"
    )
    list(APPEND FIRST_LINE_BINARY_FILES ${FIRST_LINE_BINARY})
endforeach()
add_custom_target(first_line_files DEPENDS ${FIRST_LINE_BINARY_FILES})
add_dependencies(openttd first_line_files)

# Optional single-world opening; normal New Game continues to select no script.
set(VALE_BINARY_FILES)
foreach(VALE_FILE info.nut main.nut)
    set(VALE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/openspacettd_vale/${VALE_FILE})
    set(VALE_BINARY ${CMAKE_BINARY_DIR}/game/openspacettd_vale/${VALE_FILE})
    add_custom_command(OUTPUT ${VALE_BINARY}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/game/openspacettd_vale
            COMMAND ${CMAKE_COMMAND} -E copy ${VALE_SOURCE} ${VALE_BINARY}
            MAIN_DEPENDENCY ${VALE_SOURCE}
            COMMENT "Copying The Vale Railway ${VALE_FILE}"
    )
    list(APPEND VALE_BINARY_FILES ${VALE_BINARY})
endforeach()
add_custom_target(vale_files DEPENDS ${VALE_BINARY_FILES})
add_dependencies(openttd vale_files)

# Optional Alien Frontier mission; normal New Game continues to select no script.
set(HOLLOWFROST_BINARY_FILES)
foreach(HOLLOWFROST_FILE info.nut main.nut)
    set(HOLLOWFROST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/openspacettd_hollowfrost/${HOLLOWFROST_FILE})
    set(HOLLOWFROST_BINARY ${CMAKE_BINARY_DIR}/game/openspacettd_hollowfrost/${HOLLOWFROST_FILE})
    add_custom_command(OUTPUT ${HOLLOWFROST_BINARY}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/game/openspacettd_hollowfrost
            COMMAND ${CMAKE_COMMAND} -E copy ${HOLLOWFROST_SOURCE} ${HOLLOWFROST_BINARY}
            MAIN_DEPENDENCY ${HOLLOWFROST_SOURCE}
            COMMENT "Copying The Hollowfrost Survey ${HOLLOWFROST_FILE}"
    )
    list(APPEND HOLLOWFROST_BINARY_FILES ${HOLLOWFROST_BINARY})
endforeach()
add_custom_target(hollowfrost_files DEPENDS ${HOLLOWFROST_BINARY_FILES})
add_dependencies(openttd hollowfrost_files)
