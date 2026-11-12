# DC22 case: export STL / DXF / preview for print and CNC
#   powershell -NoProfile -ExecutionPolicy Bypass -File render_dc22.ps1 -Mode all -What all
param(
  [ValidateSet('print','cnc','all')][string]$Mode = 'all',
  [ValidateSet('all','stl','dxf','preview')][string]$What = 'all'
)
$root = $PSScriptRoot
$proj = (Resolve-Path (Join-Path $root '..\..')).Path   # 项目根：app/ 便携版 OpenSCAD 仍在原位
$exe  = Join-Path $proj 'app\openscad.com'
$scad = Join-Path $root 'case_dc22.scad'
$out  = Join-Path $root 'output'
if (-not (Test-Path $exe))  { throw "missing $exe" }
if (-not (Test-Path $scad)) { throw "missing $scad" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$modes = if ($Mode -eq 'all') { @('print','cnc') } else { @($Mode) }
$names = @{ 1 = 'bottom'; 2 = 'plate'; 3 = 'top'; 8 = 'weight'; 7 = 'assembly';
            9 = 'split_base'; 10 = 'split_frame' }
foreach ($m in $modes) {
  $flag = if ($m -eq 'cnc') { 'cnc=true' } else { 'cnc=false' }
  if ($What -in @('all','stl')) {
    # 拆件版只给打印用；CNC 仍出整件
    $parts = if ($m -eq 'print') { @(1,2,3,8,7,9,10) } else { @(1,2,3,8,7) }
    foreach ($p in $parts) {
      $f = Join-Path $out "${m}_$($names[$p]).stl"
      & $exe -o $f -D $flag -D "part=$p" $scad 2>&1 | Select-Object -Last 2
      if ($LASTEXITCODE -ne 0) { throw "$m part $p failed" }
      Write-Host "OK $f ($((Get-Item $f).Length) bytes)"
    }
  }
  if ($What -in @('all','preview')) {
    # fixed camera: OpenSCAD auto-fit zooms too far out on this assembly
    $shots = @(
      @('hero',     '0,0,0,35,0,35,340',  '1600,1000', '0'),
      @('preview',  '0,0,0,55,0,25,320',  '1600,1200', '0'),
      @('exploded', '0,0,0,55,0,25,380',  '1600,1200', '6'),
      @('back',     '0,0,0,235,0,25,320', '1600,1200', '0'),
      @('pocket',   '0,0,0,235,0,25,320', '1600,1200', '1'),
      @('side',     '0,0,0,90,0,90,300',  '1600,900',  '1')
    )
    foreach ($s in $shots) {
      $f = Join-Path $out "${m}_$($s[0]).png"
      if (Test-Path $f) { Remove-Item $f -Force }
      $cam = "--camera=$($s[1])"
      $size = "--imgsize=$($s[2])"
      & $exe -o $f $cam $size -D $flag -D "part=$($s[3])" $scad 2>&1 | Select-Object -Last 1
      if (-not (Test-Path $f)) {
        Start-Sleep -Milliseconds 250
        & $exe -o $f $cam $size -D $flag -D "part=$($s[3])" $scad 2>&1 | Select-Object -Last 1
      }
      if (-not (Test-Path $f)) { throw "$m preview $($s[0]) failed" }
      Write-Host "OK $f"
    }
  }
  if ($What -in @('all','dxf')) {
    foreach ($pair in @(@(4,'plate'),@(5,'top'))) {
      $f = Join-Path $out "${m}_$($pair[1]).dxf"
      & $exe -o $f -D $flag -D "part=$($pair[0])" $scad 2>&1 | Select-Object -Last 2
      if (Test-Path $f) { Write-Host "OK $f ($((Get-Item $f).Length) bytes)" }
    }
  }
}
