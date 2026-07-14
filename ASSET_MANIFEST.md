# UI asset manifest

Every generated source PNG is retained under `assets/source_png/`.
Screen-sized cleaned PNGs are under `assets/screen_png/`.
Compiled LVGL descriptors are in `lvgl_assets/UiAssets.cpp` and declared in `UiAssets.h`.

| LVGL symbol | Processed PNG | Runtime use |
|---|---|---|
| `ui_img_icon_battery` | `icon_battery.png` | Top bar battery glyph |
| `ui_img_icon_guide` | `icon_guide.png` | Top bar guide-pulse glyph |
| `ui_img_icon_link` | `icon_link.png` | Top bar OnStep link glyph |
| `ui_img_icon_park_locked` | `icon_park_locked.png` | Top bar parked glyph |
| `ui_img_icon_park_unlocked` | `icon_park_unlocked.png` | Top bar unparked glyph |
| `ui_img_icon_slew` | `icon_slew.png` | Top bar slew-rate glyph |
| `ui_img_icon_temperature` | `icon_temperature.png` | Top bar temperature glyph |
| `ui_img_icon_wifi` | `icon_wifi.png` | Top bar Wi-Fi glyph |
| `ui_img_meridian_horizon` | `meridian_horizon.png` | Static E/S/W meridian strip |
| `ui_img_nav_align` | `nav_align.png` | ALIGN navigation glyph |
| `ui_img_nav_goto` | `nav_goto.png` | GOTO navigation glyph |
| `ui_img_nav_home` | `nav_home.png` | HOME navigation glyph |
| `ui_img_nav_menu` | `nav_menu.png` | MENU navigation glyph |
| `ui_img_panel_button` | `panel_button.png` | Bottom navigation button treatment |
| `ui_img_panel_long` | `panel_long.png` | RA/DEC panel treatment |
| `ui_img_panel_metric` | `panel_metric.png` | ALT/AZ/T-MER panel treatment |
| `ui_img_sky_radar_frame` | `sky_radar_frame.png` | Static radar/sky-position background |
