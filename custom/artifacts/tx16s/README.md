# HERELINK template for TX16S

`HERELINK-template.zip` contains the supplied EdgeTX 2.12.4 template at
`TEMPLATES/2.Personal/HERELINK.yml`. The YAML is preserved byte for byte from
the user's SD card; its SHA-256 is
`9b511b496e4c2add7a9a56bebd2a38a7b35b80f2b635e332ab390aac5bf162a8`.

Updated on 2026-11-28 from the user's `E:/TEMPLATES/2.Personal/HERELINK.yml`.
CH5 uses the momentary SH switch with the DISARM label; CH12 has no mixer source.
The USB CH5 mapping remains buttons 0/1 in Normal two-position mode. USB Pitch
axis inversion is now off; verify pitch direction during GCS calibration when
adopting this template. The remaining supplied configuration is preserved in
the archive without re-encoding the YAML.

Copy the archive's `TEMPLATES` folder to the radio's SD card root, then use
**Manage Models → New Model → 2.Personal / Personal → HERELINK** to create a
model. EdgeTX assigns the new model file automatically. The template preserves
the supplied mixes, channels and USB joystick settings. GCS axis calibration
and button actions are configured separately on each PC.

See the [Ukrainian setup guide](../../USER_GUIDE.uk.md#як-створити-модель-herelink-із-шаблону-на-tx16s)
and the [EdgeTX model management documentation](https://github.com/EdgeTX/edgetx-user-manual/blob/2.12/color-radios/select-model.md).
