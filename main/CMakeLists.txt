idf_component_register(
    SRCS "lcd_st7796.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)
