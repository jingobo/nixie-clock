PROJECT_NAME = nixie-clock
COMPONENTS = esp esp_ringbuf bootloader bootloader_support esp8266 esptool_py freertos heap log newlib lwip nvs_flash partition_table pthread spi_flash tcpip_adapter wpa_supplicant util mdns
EXTRA_COMPONENT_DIRS = $(PROJECT_PATH)
EXTRA_CFLAGS ?= -DMDNS_ENABLE_DEBUG

include $(IDF_PATH)/make/project.mk

# Update RomFS
.PHONY: romfs
romfs:
	$(PROJECT_PATH)/../win/romfs/output/release/romfs.exe $(PROJECT_PATH)/meta/web
	$(ESPTOOLPY_WRITE_FLASH) 0x100000 $(PROJECT_PATH)/meta/web.rom
