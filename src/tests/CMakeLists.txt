add_test_files(
    alternating_iterator.cpp
    bitmath_func.cpp
    enum_over_optimisation.cpp
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
    test_prefab_world_uat.cpp
    test_script_admin.cpp
    test_window_desc.cpp
    tilearea.cpp
    utf8.cpp
)

# These editbox cases install a null video driver and must run in isolated
# processes. Catch's default discovery excludes their hidden tag.
foreach(case IN ITEMS
    "Blueprint GUI captures a selected map rectangle and saves the chosen name"
    "Blueprint GUI exports the transformed selection and imports a real file"
    "Blueprint GUI reports invalid imports collisions and export failures truthfully"
    "Blueprint GUI default export remains separate from the library scan"
    "Corporate HQ GUI establishes an owned station hub and edits its reserve"
)
    string(MAKE_C_IDENTIFIER "${case}" case_name)
    add_test(NAME "${case_name}" COMMAND openttd_test "${case}")
    set_tests_properties("${case_name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endforeach()

add_test(NAME WP09_GUI_establishment_and_process_reload
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_BINARY=$<TARGET_FILE:openttd_test>"
        "-DTEST_ROOT=${CMAKE_BINARY_DIR}/Testing/wp09-gui-persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/wp09_gui_persistence.cmake")
set_tests_properties(WP09_GUI_establishment_and_process_reload PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 100 LABELS "integration;requires-baseset")
