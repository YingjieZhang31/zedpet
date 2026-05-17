.PHONY: build upload

build:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	pio run

upload:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	pio run --target upload
