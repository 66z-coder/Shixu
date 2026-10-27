param([string]$PythonExe = "", [switch]$SkipInstall)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$venvPython = Join-Path $projectRoot '.venv-ocr/Scripts/python.exe'
if (-not $SkipInstall) {
    if (-not (Test-Path -LiteralPath $venvPython)) {
        if (-not $PythonExe) {
            $PythonExe = & py -3.12 -c 'import sys; print(sys.executable)'
            if ($LASTEXITCODE -ne 0) { throw 'Install Python 3.12 or supply -PythonExe.' }
        }
        & $PythonExe -m venv (Join-Path $projectRoot '.venv-ocr')
        if ($LASTEXITCODE -ne 0) { throw 'Could not create OCR environment.' }
    }
    & $venvPython -m pip install -r (Join-Path $PSScriptRoot 'requirements-lock.txt')
    if ($LASTEXITCODE -ne 0) { throw 'OCR package installation failed.' }
}
& $venvPython (Join-Path $PSScriptRoot 'download_models.py')
if ($LASTEXITCODE -ne 0) { throw 'OCR model download failed.' }
