idf_component_register(
    SRCS "src/board_manager.c" "src/commands.c" "src/json_encode_decode.c"  "src/gpios_manager.c" "src/provisioning.c" "src/ble_provisioning.c" "src/lab_discovery.c" 
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager nvs_manager driver app_update esp_https_ota esp_hw_support esp_wifi esp_netif esp_event nvs_flash lwip 
)