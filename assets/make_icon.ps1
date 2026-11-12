# Builds assets/dor2.ico: an original multi-resolution icon (no game art).
# Requires Windows PowerShell + System.Drawing.

Add-Type -AssemblyName System.Drawing

$outDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngs = @()

foreach ($size in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 18, 18, 22),
        [System.Drawing.Color]::FromArgb(255, 48, 12, 18),
        45.0)
    $g.FillRectangle($bg, $rect)

    # Ring ropes.
    $rope = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(210, 196, 154, 62),
        [Math]::Max(1.0, $size * 0.045))
    for ($i = 0; $i -lt 3; $i++) {
        $y = $size * (0.72 + $i * 0.085)
        $g.DrawLine($rope, $size * 0.10, $y, $size * 0.90, $y)
    }

    # Corner posts.
    $post = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(220, 120, 126, 138),
        [Math]::Max(1.0, $size * 0.05))
    $g.DrawLine($post, $size * 0.12, $size * 0.70, $size * 0.12, $size * 0.94)
    $g.DrawLine($post, $size * 0.88, $size * 0.70, $size * 0.88, $size * 0.94)

    # Wordmark: "DOR2" in championship gold, auto-fit so the "2" never clips.
    $text = "DOR2"
    $fontSize = [Math]::Max(4.0, $size * 0.34)
    $maxWidth = $size * 0.86
    $font = $null
    while ($true) {
        $font = New-Object System.Drawing.Font("Arial Black", $fontSize,
            [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $measured = $g.MeasureString($text, $font)
        if ($measured.Width -le $maxWidth -or $fontSize -le 4.0) { break }
        $font.Dispose()
        $fontSize *= 0.92
    }
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
    $textRect = [System.Drawing.RectangleF]::new(
        [single]0, [single]($size * 0.06), [single]$size, [single]($size * 0.56))
    $shadowRect = [System.Drawing.RectangleF]::new(
        [single]1, [single]($size * 0.06 + 1), [single]$size, [single]($size * 0.56))
    $shadow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(180, 0, 0, 0))
    $g.DrawString($text, $font, $shadow, $shadowRect, $fmt)
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 232, 181, 58))
    $g.DrawString($text, $font, $brush, $textRect, $fmt)

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,($ms.ToArray())

    $g.Dispose(); $bmp.Dispose(); $bg.Dispose(); $rope.Dispose(); $post.Dispose()
    $font.Dispose(); $shadow.Dispose(); $brush.Dispose()
}

# ICO container with PNG-compressed entries.
$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write([UInt16]0)             # reserved
$writer.Write([UInt16]1)             # type: icon
$writer.Write([UInt16]$sizes.Count)  # image count
$offset = 6 + (16 * $sizes.Count)

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $writer.Write([Byte]$dim)        # width
    $writer.Write([Byte]$dim)        # height
    $writer.Write([Byte]0)           # palette
    $writer.Write([Byte]0)           # reserved
    $writer.Write([UInt16]1)         # planes
    $writer.Write([UInt16]32)        # bpp
    $writer.Write([UInt32]$pngs[$i].Length)
    $writer.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($png in $pngs) { $writer.Write($png) }
$writer.Flush()

$dest = Join-Path $outDir "dor2.ico"
[System.IO.File]::WriteAllBytes($dest, $stream.ToArray())
$writer.Dispose(); $stream.Dispose()

Write-Host "Wrote $dest ($($sizes.Count) sizes)"
