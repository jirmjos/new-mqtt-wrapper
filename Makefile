.PHONY: all
all:
	pio run
.PHONY: flash
flash:
	pio run --target upload
