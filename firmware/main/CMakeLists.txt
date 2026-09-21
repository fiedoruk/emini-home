set(ui_files)
set(HOME_UI_DIR "${CMAKE_CURRENT_LIST_DIR}/../ui" CACHE PATH "Exact local UI source directory")
set(HOME_BOARD "note4c" CACHE STRING "Hardware target: note4c or nm-epd-420-4c")
set_property(CACHE HOME_BOARD PROPERTY STRINGS note4c nm-epd-420-4c)
if(HOME_BOARD STREQUAL "note4c")
    set(home_board_sources "home_board.c" "home_battery.c" "home_panel.c")
elseif(HOME_BOARD STREQUAL "nm-epd-420-4c")
    set(home_board_sources "home_board_nm_epd_420_4c.c"
        "home_battery_nm_epd_420_4c.c" "home_panel_nm_epd_420_4c.c")
else()
    message(FATAL_ERROR "Unsupported HOME_BOARD: ${HOME_BOARD}")
endif()
foreach(name index.html device.css device.js core.js)
    set(asset "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    configure_file("${HOME_UI_DIR}/${name}" "${asset}" COPYONLY)
    list(APPEND ui_files "${asset}")
endforeach()
idf_component_register(SRCS
    "home_main.c" ${home_board_sources} "home_config.c" "home_store.c" "home_network.c" "home_server.c"
    "home_fetch.c" "home_parse.c" "home_render.c" "home_location.c" "generated/home_font.c"
    "home_places.c" "generated/home_zones.c" "home_discovery.c" "home_usb.c"
    "home_parse_air.c" "home_sky.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_timer esp_system esp_event
    esp_netif esp_wifi esp_http_server esp-tls mbedtls home_json nvs_flash
    bootloader_support esp_psram mdns home_qr esp_adc esp_app_format
    EMBED_FILES ${ui_files})
if(HOME_TESTCARD)
    # Measurement build only (plan 0.5.0 step 0.1): idf.py -DHOME_TESTCARD=1
    target_compile_definitions(${COMPONENT_LIB} PRIVATE HOME_TESTCARD=1)
endif()
