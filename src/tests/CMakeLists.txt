add_test_files(
    test_commonwealth_construction_policy.cpp
    test_first_line_funding.cpp
    test_first_line_approach.cpp
    test_first_permanent_line.cpp
    test_world0_opening.cpp
    test_world0_startup.cpp
    test_world0_routes.cpp
    test_world0_script.cpp
    test_train_purchase_compatibility.cpp
    test_commonwealth_works.cpp
    test_core_construction_basket.cpp
    test_fleet_shuttle_growth.cpp
    test_integrated_economy.cpp
    test_integrated_town_generation.cpp
    test_stellar_network.cpp
    test_resource_sites.cpp
    alternating_iterator.cpp
    bitmath_func.cpp
    enum_over_optimisation.cpp
    test_tooltips.cpp
    test_error_gui.cpp
    test_command_authority.cpp
    test_command_authority_network.cpp
    flatset_type.cpp
    history_func.cpp
    landscape_partial_pixel_z.cpp
    math_func.cpp
    mock_environment.h
    mock_fontcache.h
    mock_spritecache.cpp
    mock_spritecache.h
    string_builder.cpp
    string_consumer.cpp
    string_inplace.cpp
    string_func.cpp
    test_main.cpp
    test_network_crypto.cpp
    test_placement_restrictions.cpp
    test_planet_manager.cpp
    test_planet_ui.cpp
    test_saveload_planet.cpp
    test_portal_wormhole.cpp
    test_consist_traversal.cpp
    test_federation.cpp
    test_authority_request_queue.cpp
    test_federation_reliability.cpp
    test_federation_transfer.cpp
    test_federation_persistent_universe.cpp
    test_federation_megacity_economy.cpp
    test_megacity_gui.cpp
    test_federation_cluster.cpp
    test_federation_order_restoration.cpp
    test_portal_construction.cpp
    test_world_gen.cpp
    test_interplanetary_cargo.cpp
    test_spaceports_and_conduits.cpp
    test_spaceport_conduit_bridge.cpp
    test_gateway_arrays_and_telemetry.cpp
    test_sprint24_alien_biomes.cpp
    test_blueprint.cpp
    test_blueprint_validation.cpp
    test_blueprint_storage.cpp
    test_blueprint_file_gui.cpp
    test_cst_prefabs.cpp
    test_sprint27_feature_ui.cpp
    test_sprint28_guided_uat.cpp
    test_sprint29_federation_acceptance.cpp
    test_sprint30_biomes_colonization.cpp
    test_sprint31_colonization_gui.cpp
    test_sprint32_lifecycle_and_tech.cpp
    test_sprint33_planetary_economy_and_megacity.cpp
    test_sprint36_all_features_uat.cpp
    test_sprint37_commonwealth_pack.cpp
    test_sprint39_corporate_hq_and_stockpile.cpp
    test_sprint40_fabrication_engine.cpp
    test_sprint41_tech_tree.cpp
    test_sprint42_production_chains.cpp
    test_sprint43_empire_facilities.cpp
    test_sprint44_lore_ai_competitors.cpp
    test_sprint45_visual_overhaul.cpp
    test_sprint46_live_federation.cpp
    test_sprint47_alliances_and_neutral_tracks.cpp
    test_sprint48_prompt_to_savegame.cpp
    test_sprint48_balancing_critic.cpp
    test_universe_graph.cpp
    test_prebuilt_trade.cpp
    test_sprint50_throat_and_staging.cpp
    test_sprint50_universal_and_charters.cpp
    test_prefab_world_uat.cpp
    test_script_admin.cpp
    test_window_desc.cpp
    test_electronics_loop.cpp
    test_gate_sprites.cpp
    tilearea.cpp
    utf8.cpp
)

if(NOT OPTION_DEDICATED AND NOT EMSCRIPTEN)
    add_test_files(test_agent_uat_native.cpp)
endif()

if(WITH_CAB_VIEW AND NOT OPTION_DEDICATED AND NOT EMSCRIPTEN)
    add_test_files(test_cab_view.cpp test_cab_view_native.cpp test_cab_view_gate_native.cpp)
    add_test(NAME Cab_view_native_pixel_buffer
        COMMAND openttd_test "Cab CPU renderer sky output resizes without simulation mutation")
    set_tests_properties(Cab_view_native_pixel_buffer PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 60 LABELS "cab-view;integration")
    add_test(NAME Cab_view_entry_policy
        COMMAND openttd_test "Cab entry rejects foreign nonprimary and unsupported train states")
    set_tests_properties(Cab_view_entry_policy PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 60 LABELS "cab-view;integration")
    add_test(NAME Cab_view_window_lifecycle
        COMMAND openttd_test "Cab window is singleton and closes on session change or train deletion")
    set_tests_properties(Cab_view_window_lifecycle PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 60 LABELS "cab-view;integration")
    add_test(NAME Cab_view_local_scene_mask
        COMMAND openttd_test "Cab CPU renderer draws local terrain but masks foreign terrain and vehicles")
    set_tests_properties(Cab_view_local_scene_mask PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 60 LABELS "cab-view;integration;requires-baseset")
endif()

# These editbox cases install a null video driver and must run in isolated
# processes. Catch's default discovery excludes their hidden tag.
foreach(case IN ITEMS
    "HQ Tech Tree rendering leaves absent and existing research state unchanged"
    "Tooltips reject empty rendered text and preserve visible help"
    "Blueprint GUI captures a selected map rectangle and saves the chosen name"
    "Blueprint GUI exports the transformed selection and imports a real file"
    "Blueprint GUI reports invalid imports collisions and export failures truthfully"
    "Blueprint GUI default export remains separate from the library scan"
    "Blueprint GUI unblocks CST placement with empty stockpiles and no HQ"
    "Corporate HQ GUI establishes an owned station hub and edits its reserve"
)
    string(MAKE_C_IDENTIFIER "${case}" case_name)
    add_test(NAME "${case_name}" COMMAND openttd_test "${case}")
    set_tests_properties("${case_name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    if(case STREQUAL "HQ Tech Tree rendering leaves absent and existing research state unchanged")
        set_tests_properties("${case_name}" PROPERTIES ENVIRONMENT "OSTTD_TEST_BINARY=$<TARGET_FILE:openttd_test>"
            LABELS "integration;requires-baseset")
    endif()
endforeach()

# Vale FIFO cases install a test-local video driver. Keep each in its own
# process so the native graphics singleton cannot leak into other fixtures.
if(UNIX)
    foreach(case IN ITEMS
        "Vale startup bridge defers a real FIFO exec beyond modal cleanup"
        "Vale bridge rejects malformed paths and unrelated startup states without mutation"
        "Vale queued bridge rechecks native admission before opening its FIFO"
        "Vale bridge admits only a validated loaded World 0 opening"
        "Vale completion emits its native briefing and receipt without a main viewport"
        "Vale completion replaces editor chrome and scrolls a real main viewport"
    )
        string(MAKE_C_IDENTIFIER "${case}" case_name)
        add_test(NAME "${case_name}" COMMAND openttd_test "${case}")
        set_tests_properties("${case_name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            TIMEOUT 30 LABELS "integration;world0-startup")
        if(case STREQUAL "Vale completion replaces editor chrome and scrolls a real main viewport")
            set_tests_properties("${case_name}" PROPERTIES
                ENVIRONMENT "OSTTD_TEST_BINARY=$<TARGET_FILE:openttd_test>"
                LABELS "integration;requires-baseset;world0-startup")
        endif()
    endforeach()
endif()

add_test(NAME WP09_GUI_establishment_and_process_reload
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_BINARY=$<TARGET_FILE:openttd_test>"
        "-DTEST_ROOT=${CMAKE_BINARY_DIR}/Testing/wp09-gui-persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/wp09_gui_persistence.cmake")
set_tests_properties(WP09_GUI_establishment_and_process_reload PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 100 LABELS "integration;requires-baseset")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME Commonwealth_construction_policy_cold_process_reload
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/test_commonwealth_construction_policy.py"
            "$<TARGET_FILE:openttd_test>" --artifact-root "${CMAKE_BINARY_DIR}/Testing/construction-policy")
    set_tests_properties(Commonwealth_construction_policy_cold_process_reload PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 280 LABELS "integration;requires-baseset;construction-policy")
    add_test(NAME Stellar_directory_authentication_and_restart
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/test_stellar_directory.py")
    if(UNIX)
        foreach(mode IN ITEMS classic v1 v2)
            add_test(NAME Commonwealth_construction_policy_native_relay_${mode}
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/test_command_authority_network.py"
                    "$<TARGET_FILE:openttd_test>" --construction-policy ${mode})
            set_tests_properties(Commonwealth_construction_policy_native_relay_${mode} PROPERTIES
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 90 LABELS "integration;construction-policy")
        endforeach()
        add_test(NAME Megacity_designation_native_relay_and_reload
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/test_command_authority_network.py"
                "$<TARGET_FILE:openttd_test>" --megacity-designation)
        set_tests_properties(Megacity_designation_native_relay_and_reload PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 90 LABELS "integration;command-authority")
    endif()
endif()
