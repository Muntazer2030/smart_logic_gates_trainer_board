idf_component_register(
    SRCS "src/board_manager.c" "src/commands.c" "src/json_encode_decode.c"  "src/gpios_manager.c" 
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager nvs_manager driver json app_update esp_https_ota 
)