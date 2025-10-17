idf_component_register(
    SRCS "src/nvs_manager.c" "src/nvs_api.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)