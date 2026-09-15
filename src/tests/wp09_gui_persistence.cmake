# Run native GUI callbacks, then reload with real base graphics and engine
# startup in a clean process. Requires base graphics in a normal OpenTTD data
# folder or build/baseset; packed .tar sets are supported too.
if(NOT DEFINED TEST_BINARY OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "TEST_BINARY and TEST_ROOT are required")
endif()
file(MAKE_DIRECTORY "${TEST_ROOT}")
set(save_path "${TEST_ROOT}/fresh-company.sav")
foreach(case IN ITEMS
    "Corporate HQ GUI lets an eligible player found the first HQ"
    "WP09 fresh HQ hub and reserve survive process reload"
)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "OSTTD_WP09_SAVE_PATH=${save_path}"
            "OSTTD_TEST_BINARY=${TEST_BINARY}"
            "${TEST_BINARY}" "${case}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 45
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${case} failed (${result})\n${output}\n${errors}\nSave retained at ${save_path}")
    endif()
    message(STATUS "${case}: passed")
endforeach()
file(REMOVE "${save_path}")
