SDK_BASE        ?= C:/Programs/Espressif/ESP8266/sdk/rtos
BUILD_PATH_BASE	?= C:/Programs/Espressif/ESP8266/build

# Main settings includes
include	$(BUILD_PATH_BASE)/settings.mk

BOOT            = new
APP		        = 0
SPI_SPEED       = 40
SPI_MODE        = DIO
SPI_SIZE_MAP    = 2
ESPPORT         = COM4
ESPBAUD         = 921600

MODULES	= source
LIBS	= gcc driver hal phy pp net80211 wpa crypto main freertos lwip minic smartconfig

# Root includes
include	$(BUILD_PATH_BASE)/common_rtos.mk
