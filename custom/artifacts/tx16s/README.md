# HERELINK template for TX16S

`HERELINK-template.zip` contains the supplied EdgeTX 2.12.4 template at
`TEMPLATES/2.Personal/HERELINK.yml`. The YAML is preserved byte for byte from
the user's SD card; its SHA-256 is
`5b20fb8c254b7da1b93da5e0b284451321bb19b9a22781b172e9cbae9d0603fb`.

Copy the archive's `TEMPLATES` folder to the radio's SD card root, then use
**Manage Models → New Model → 2.Personal / Personal → HERELINK** to create a
model. EdgeTX assigns the new model file automatically. The template preserves
the supplied mixes, channels and USB joystick settings. GCS axis calibration
and button actions are configured separately on each PC.

See the [Ukrainian setup guide](../../USER_GUIDE.uk.md#як-створити-модель-herelink-із-шаблону-на-tx16s)
and the [EdgeTX model management documentation](https://github.com/EdgeTX/edgetx-user-manual/blob/2.12/color-radios/select-model.md).
