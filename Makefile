.PHONY: build upload

build:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	@test -n "$$CLAUDE_SERVER" || { echo "ERROR: CLAUDE_SERVER not set (e.g. http://192.168.1.42:8000)"; exit 1; }
	pio run

upload:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	@test -n "$$CLAUDE_SERVER" || { echo "ERROR: CLAUDE_SERVER not set (e.g. http://192.168.1.42:8000)"; exit 1; }
	pio run --target upload
