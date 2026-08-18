# mipi_rgb display model for the DIYLESS Thermostat 3.
# Pin mapping and RGB timing are taken from the board's stock ESPHome config:
# https://github.com/diyless/esphome-opentherm-thermostat/blob/main/diyless-thermostat-3.yaml

from .st7701s import st7701s

st7701s.extend(
    "DIYLESS-THERMOSTAT-3",
    width=480,
    height=480,
    invert_colors=True,
    # This board wires the same 16 data lines (5 red, 6 green, 5 blue) as the
    # MAKERFABS-4 model below, which needs 18-bit pixel mode for that exact
    # wiring to map correctly; 16-bit mode shows as a green tint/glow.
    pixel_mode="18bit",
    cs_pin=1,
    reset_pin=43,
    de_pin={"number": 45, "ignore_strapping_warning": True},
    hsync_pin=5,
    vsync_pin=4,
    pclk_pin=21,
    pclk_frequency="10MHz",
    pclk_inverted=False,
    hsync_pulse_width=14,
    hsync_back_porch=14,
    hsync_front_porch=14,
    vsync_pulse_width=10,
    vsync_back_porch=14,
    vsync_front_porch=14,
    data_pins={
        "red": [39, 40, 41, 42, 2],
        "green": [0, 9, 14, 47, 48, 3],
        "blue": [6, 7, 15, 16, 8],
    },
)
