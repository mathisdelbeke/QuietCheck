idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS ""
                    REQUIRES 
                        esp_adc
                        mqtt
                        nvs_flash 
                        esp_wifi 
                        esp_event 
                        mqtt 
                        esp_netif
                    )
