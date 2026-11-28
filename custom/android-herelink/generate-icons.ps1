$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source = Join-Path $repo 'custom/resources/herelink-gcs.ico'
$ico = [System.IO.File]::ReadAllBytes($source)
$count = [BitConverter]::ToUInt16($ico, 4)
$frames = for ($index = 0; $index -lt $count; $index++) {
    $entry = 6 + 16 * $index
    $width = if ($ico[$entry]) { [int]$ico[$entry] } else { 256 }
    $height = if ($ico[$entry + 1]) { [int]$ico[$entry + 1] } else { 256 }
    [PSCustomObject]@{
        Area = $width * $height
        Length = [BitConverter]::ToUInt32($ico, $entry + 8)
        Offset = [BitConverter]::ToUInt32($ico, $entry + 12)
    }
}
$largest = $frames | Sort-Object Area -Descending | Select-Object -First 1
$frame = [byte[]]::new($largest.Length)
[Array]::Copy($ico, $largest.Offset, $frame, 0, $largest.Length)
if ([BitConverter]::ToString($frame, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A') {
    throw 'The desktop icon must contain a PNG frame at its largest resolution'
}

# Read the embedded PNG directly: Icon.ToBitmap can select a smaller ICO frame.
$stream = [System.IO.MemoryStream]::new($frame, $false)
$image = [System.Drawing.Image]::FromStream($stream)
$sizes = @{ ldpi = 36; mdpi = 48; hdpi = 72; xhdpi = 96; xxhdpi = 144; xxxhdpi = 192 }
try {
    foreach ($density in $sizes.Keys) {
        $size = $sizes[$density]
        $directory = Join-Path $PSScriptRoot "icons/drawable-$density"
        [System.IO.Directory]::CreateDirectory($directory) | Out-Null
        $path = Join-Path $directory 'icon.png'
        $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $output = [System.IO.MemoryStream]::new()
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($image, [System.Drawing.Rectangle]::new(0, 0, $size, $size))
            $bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
            $bytes = $output.ToArray()
            if (!(Test-Path -LiteralPath $path) -or
                [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($path)) -ne [Convert]::ToBase64String($bytes)) {
                [System.IO.File]::WriteAllBytes($path, $bytes)
            }
        } finally {
            $output.Dispose()
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }
} finally {
    $image.Dispose()
    $stream.Dispose()
}
