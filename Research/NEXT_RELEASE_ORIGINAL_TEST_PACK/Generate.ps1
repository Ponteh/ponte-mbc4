param([string]$Python = '', [switch]$Force)
$ErrorActionPreference = 'Stop'
if (-not $Python) {
    $portable = Join-Path $PSScriptRoot '../../../../build/mc2000-meter-analysis-tools/python/python.exe'
    if (Test-Path -LiteralPath $portable) { $Python = $portable } else { $Python = 'python' }
}
$generatorArgs = @((Join-Path $PSScriptRoot 'generate.py'))
if ($Force) { $generatorArgs += '--force' }
& $Python @generatorArgs
if ($LASTEXITCODE -ne 0) { throw "Generation failed: $LASTEXITCODE" }
