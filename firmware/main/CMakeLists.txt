set(ui_files)
set(HOME_UI_DIR "${CMAKE_CURRENT_LIST_DIR}/../ui" CACHE PATH "Exact local UI source directory")
foreach(name index.html device.css device.js core.js)
    set(asset "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    configure_file("${HOME_UI_DIR}/${name}" "${asset}" COPYONLY)
    list(APPEND ui_files "${asset}")
endforeach()
idf_component_register(SRCS
    "home_main.c" "home_config.c" "home_store.c" "home_network.c" "home_server.c"
    "home_fetch.c" "home_parse.c" "home_render.c" "home_location.c" "generated/home_font.c"
    "home_places.c" "generated/home_zones.c" "home_discovery.c" "home_usb.c" "home_battery.c"
    "home_parse_air.c" "home_sky.c" "home_power.c" "home_wake.c"
    "home_panel.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_timer esp_system esp_event esp_pm
    esp_driver_usb_serial_jtag
    esp_netif esp_wifi esp_http_server esp-tls mbedtls home_json nvs_flash
    bootloader_support esp_psram mdns home_qr esp_adc esp_app_format
    EMBED_FILES ${ui_files})
if(HOME_PM_PROFILE)
    # Diagnostic build only: idf.py -DHOME_PM_PROFILE=1
    # Prints the power management locks once a minute and lets the device sleep on a cable,
    # so the sleep can be watched over the serial console. Never released.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE HOME_PM_PROFILE=1)
endif()
if(HOME_TESTCARD)
    # Measurement build only: idf.py -DHOME_TESTCARD=1
    target_compile_definitions(${COMPONENT_LIB} PRIVATE HOME_TESTCARD=1)
endif()
