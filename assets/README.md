# Image assets

`source_png/` contains the original generated source images with stable descriptive names.

`screen_png/` contains cleaned, cropped and resized PNGs at their actual embedded UI dimensions. These are the files converted into `UiAssets.cpp`.

The generated source images sometimes contained a visible checkerboard rather than a true alpha channel. The screen-sized versions were processed to create usable transparency before conversion.
