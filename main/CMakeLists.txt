idf_component_register(
    SRCS
        main.c
        config/config.c
        route/config.c
        route/index.c
        wifi/ap.c
        wifi/sta.c
        mqtt/mqtt.c
        dl/dl_wrapper.cpp

    EMBED_FILES 
        model/best.espdl

    PRIV_INCLUDE_DIRS
        .

    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_psram
		esp_timer
        esp_http_server
        mqtt
        littlefs
        esp32-camera
)

# Suppress noisy warnings for designated initializers when compiling as C++
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)