ENV ?= ideaspark

.PHONY: build build-all upload monitor deploy ports clean

# Carte principale : ideaspark ESP32-WROOM-32 + écran ST7789 intégré.
build:
	pio run -e $(ENV)

build-all:
	pio run -e ideaspark
	pio run -e esp32dev
	pio run -e esp32s3

# PlatformIO détecte automatiquement /dev/cu.usbserial* ou /dev/cu.wchusbserial*.
upload:
	pio run -e $(ENV) -t upload

monitor:
	pio device monitor -e $(ENV)

# Compile, flashe, puis ouvre le journal série. Ctrl-C ferme le moniteur.
deploy: upload
	pio device monitor -e $(ENV)

ports:
	pio device list

clean:
	pio run -e $(ENV) -t clean
