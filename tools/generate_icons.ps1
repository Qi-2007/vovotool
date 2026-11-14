# Regenerate Android launcher icons from the original logo without extra dependencies.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = [System.Drawing.Image]::FromFile((Join-Path $projectRoot 'logo.png'))
try {
    $sizes = [ordered]@{ mdpi = 48; hdpi = 72; xhdpi = 96; xxhdpi = 144; xxxhdpi = 192 }
    foreach ($density in $sizes.Keys) {
        $size = $sizes[$density]
        $directory = Join-Path $projectRoot "app/src/main/res/mipmap-$density"
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $bitmap = [System.Drawing.Bitmap]::new($size, $size)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
                try {
                    $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
                    $rectangle = [System.Drawing.Rectangle]::new(0, 0, $size, $size)
                    $graphics.DrawImage($source, $rectangle, 0, 0, $source.Width, $source.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
                } finally { $attributes.Dispose() }
            } finally { $graphics.Dispose() }
            $output = Join-Path $directory 'ic_launcher.png'
            $bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
            Write-Output "$density ${size}x${size}: $output"
        } finally { $bitmap.Dispose() }
    }
} finally { $source.Dispose() }
