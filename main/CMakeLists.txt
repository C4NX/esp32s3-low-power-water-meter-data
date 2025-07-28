idf_component_register(
    SRCS
        main.c
        config/config.c
        route/config.c
        route/index.c
        wifi/ap.c
        wifi/sta.c
        mqtt/mqtt.c

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
)