idf_component_register(
    SRCS "src/wifi_switch_manager.c" "src/commands.c" "src/json_encode_decode.c" "src/nvs_manager_api.c" "src/gpios_manager.c" "src/time_manager.c" "src/timer.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager nvs_manager driver json app_update esp_https_ota 
)