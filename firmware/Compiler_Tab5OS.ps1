# ASCII source, compatible with Windows PowerShell 5.1 and PowerShell 7.
# Creates a separate build environment beside this script. No Arduino15
# removal, global library edit, flashing, C6 update or ESP-IDF patch.
param(
    [string]$ArduinoCli = 'arduino-cli',
    [switch]$InstallDependencies,
    [string]$SketchPath = '',
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($SketchPath) {
    $AppPath = [IO.Path]::GetFullPath($SketchPath)
    $AppName = Split-Path -Leaf $AppPath
} else {
    $Candidates = @(Get-ChildItem -LiteralPath $PSScriptRoot -Directory | Where-Object {
        $_.Name -match '^Tab5OS_Arduino_v\d+\.\d+\.\d+$' -and
        (Test-Path -LiteralPath (Join-Path $_.FullName ($_.Name + '.ino')))
    })
    if ($Candidates.Count -ne 1) {
        throw 'Expected one Tab5OS sketch beside this script. Use -SketchPath when several versions are present.'
    }
    $AppPath = $Candidates[0].FullName
    $AppName = $Candidates[0].Name
}
if ($AppName -notmatch '^Tab5OS_Arduino_v(\d+\.\d+\.\d+)$' -or
    -not (Test-Path -LiteralPath (Join-Path $AppPath ($AppName + '.ino')))) {
    throw 'The selected directory is not a complete Tab5OS sketch.'
}
$AppVersion = $Matches[1]
if ($CheckOnly) {
    [PSCustomObject]@{ Sketch = $AppName; Version = $AppVersion; Path = $AppPath }
    return
}
if ($ArduinoCli -eq 'arduino-cli') {
    # Arduino PLC IDE may expose an unrelated CLI first in PATH.
    foreach ($Candidate in @(
        (Join-Path $env:ProgramFiles 'Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe')
    )) {
        if (Test-Path -LiteralPath $Candidate) { $ArduinoCli = $Candidate; break }
    }
}
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
    throw 'Arduino CLI absent. Install it or pass -ArduinoCli with its full path.'
}
$BuildEnv = Join-Path $PSScriptRoot 'build-environment'
$UserDir = Join-Path $BuildEnv 'sketchbook'
$DataDir = Join-Path $BuildEnv 'data'
$LogsDir = Join-Path $BuildEnv 'logs'
$SavedEnvironment = @{}
$Overrides = @{
    ARDUINO_DIRECTORIES_DATA = $DataDir
    ARDUINO_DIRECTORIES_USER = $UserDir
    ARDUINO_DIRECTORIES_DOWNLOADS = (Join-Path $BuildEnv 'downloads')
    ARDUINO_BUILD_CACHE_PATH = (Join-Path $BuildEnv 'cache')
    ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS = 'https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json'
}
function Assert-BuildPath {
    param([string]$Candidate)
    $ExpectedRoot = [IO.Path]::GetFullPath($BuildEnv).TrimEnd('\') + '\'
    $ResolvedCandidate = [IO.Path]::GetFullPath($Candidate)
    if (-not $ResolvedCandidate.StartsWith($ExpectedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw ('Refusing operation outside isolated build environment: ' + $ResolvedCandidate)
    }
    # Reject junctions/symlinks on the candidate or any parent down to the script.
    $ExistingPath = $ResolvedCandidate
    $ScriptRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
    while ($ExistingPath -and $ExistingPath.Length -ge $ScriptRoot.Length) {
        if (Test-Path -LiteralPath $ExistingPath) {
            $Item = Get-Item -LiteralPath $ExistingPath -Force
            if (($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw ('Refusing operation through a filesystem link: ' + $ExistingPath)
            }
        }
        if ($ExistingPath -eq $ScriptRoot) { break }
        $ExistingPath = [IO.Path]::GetDirectoryName($ExistingPath)
    }
}
function Invoke-Arduino {
    param([string[]]$CliArgs)
    & $ArduinoCli @CliArgs
    if ($LASTEXITCODE -ne 0) {
        throw ('Arduino CLI failed: ' + ($CliArgs -join ' '))
    }
}
try {
    foreach ($Key in $Overrides.Keys) {
        $SavedEnvironment[$Key] = [Environment]::GetEnvironmentVariable($Key, 'Process')
        [Environment]::SetEnvironmentVariable($Key, $Overrides[$Key], 'Process')
    }
    foreach ($Directory in @($BuildEnv, $DataDir, $UserDir, $LogsDir)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }
    if ($InstallDependencies) {
        Invoke-Arduino -CliArgs @('core', 'update-index')
        Invoke-Arduino -CliArgs @('core', 'install', 'm5stack:esp32@3.3.9')
        Invoke-Arduino -CliArgs @('lib', 'update-index')
        foreach ($Library in @('M5GFX@0.2.30', 'M5Unified@0.2.23', 'lvgl@9.6.0', 'ArduinoJson@7.4.3')) {
            Invoke-Arduino -CliArgs @('lib', 'install', '--no-deps', $Library)
        }
        # The upstream 2.4.2 tag still declares 2.4.1 in library.properties.
        # Fetch the fixed tag directly while the registry ZIP is unavailable.
        $AudioZip = Join-Path $BuildEnv 'ESP8266Audio-2.4.2.zip'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://codeload.github.com/earlephilhower/ESP8266Audio/zip/refs/tags/2.4.2' -OutFile $AudioZip
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $AudioZip).Hash.ToLowerInvariant() -ne '80e7a77de974e41f7cc73020067912b2eb38d991bcd170a9c3734c8ea61c7224') { throw 'ESP8266Audio archive checksum mismatch.' }
        $AudioStage = Join-Path $BuildEnv 'audio-staging'
        Assert-BuildPath -Candidate $AudioStage
        if (Test-Path -LiteralPath $AudioStage) { Remove-Item -LiteralPath $AudioStage -Recurse -Force }
        Expand-Archive -LiteralPath $AudioZip -DestinationPath $AudioStage
        $AudioLibrary = Join-Path $UserDir 'libraries\ESP8266Audio'
        Assert-BuildPath -Candidate $AudioLibrary
        Assert-BuildPath -Candidate (Join-Path $AudioStage 'ESP8266Audio-2.4.2')
        if (Test-Path -LiteralPath $AudioLibrary) { Remove-Item -LiteralPath $AudioLibrary -Recurse -Force }
        Move-Item -LiteralPath (Join-Path $AudioStage 'ESP8266Audio-2.4.2') -Destination $AudioLibrary
        # Keep upstream metadata unchanged; the tag is identified by its archive hash.
    }
    $CoreDir = Join-Path $DataDir 'packages\m5stack\hardware\esp32\3.3.9'
    if (-not (Test-Path -LiteralPath $CoreDir)) {
        throw 'Isolated core missing. Run this script once with -InstallDependencies.'
    }
    $LibrariesDir = Join-Path $UserDir 'libraries'
    New-Item -ItemType Directory -Force -Path $LibrariesDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $AppPath 'lv_conf.h') -Destination (Join-Path $LibrariesDir 'lv_conf.h') -Force
    # Apply the same bounded Arabic allocation fix as the delivered firmware,
    # using a verified separate library copy. The installed library is unchanged.
    $LvglHelper = Join-Path $PSScriptRoot 'build\build_clang\prepare_lvgl.py'
    $LvglSource = Join-Path $LibrariesDir 'lvgl'
    $LvglWork = Join-Path $BuildEnv 'lvgl-patched'
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python 3 is required to prepare the isolated LVGL library.' }
    & python $LvglHelper --source $LvglSource --work $LvglWork
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the verified LVGL Arabic allocation fix.' }
    $LvglLibrary = Join-Path $LvglWork 'dependencies\lvgl'
    $JpegHelper = Join-Path $PSScriptRoot 'build\build_linux\prepare_m5gfx_jpeg.py'
    $JpegSource = Join-Path $LibrariesDir 'M5GFX'
    $JpegWork = Join-Path $BuildEnv 'm5gfx-jpeg-patched'
    & python $JpegHelper --source $JpegSource --work $JpegWork
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the verified JPEG decoder fixes.' }
    $M5GfxLibrary = Join-Path $JpegWork 'dependencies\M5GFX'
    $SpeakerHelper = Join-Path $PSScriptRoot 'build\build_linux\prepare_m5unified_audio.py'
    $SpeakerSource = Join-Path $LibrariesDir 'M5Unified'
    $SpeakerWork = Join-Path $BuildEnv 'm5unified-audio-patched'
    & python $SpeakerHelper --source $SpeakerSource --work $SpeakerWork
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the verified M5Unified audio startup fix.' }
    $M5UnifiedLibrary = Join-Path $SpeakerWork 'dependencies\M5Unified'
    $FsHelper = Join-Path $PSScriptRoot 'build\build_linux\prepare_fs_errno.py'
    $FsSource = Join-Path $CoreDir 'libraries\FS'
    $FsWork = Join-Path $BuildEnv 'fs-errno-patched'
    & python $FsHelper $FsSource $FsWork
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the verified filesystem errno backport.' }
    $FsLibrary = Join-Path $FsWork 'dependencies\FS'
    $DsiHelper = Join-Path $PSScriptRoot 'build\build_linux\prepare_dsi_cache_safe.py'
    $DsiSdk = Join-Path $DataDir 'packages\m5stack\tools\esp32p4_es-libs\3.3.9'
    $DsiCompiler = Join-Path $DataDir 'packages\m5stack\tools\esp-rv32\2601\bin\riscv32-esp-elf-gcc.exe'
    $DsiWork = Join-Path $BuildEnv 'dsi-cache-safe-patched'
    $DsiArchivesJson = & python $DsiHelper --sdk-root $DsiSdk --gcc $DsiCompiler --work $DsiWork
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare cache-safe native DSI/DW-GDMA units.' }
    $DsiArchives = @($DsiArchivesJson | ConvertFrom-Json)
    $HeapProbeHelper = Join-Path $PSScriptRoot 'build\build_linux\verify_heap_probe.py'
    $HeapProbeLog = Join-Path $LogsDir 'heap-probe-sdk.json'
    $HeapProbeJson = & python $HeapProbeHelper --sdk-root $DsiSdk --output $HeapProbeLog
    $HeapProbeDefine = '-DTAB5_HEAP_PROBE_PINNED=0'
    if ($LASTEXITCODE -eq 0) {
        $HeapProbe = $HeapProbeJson | ConvertFrom-Json
        $HeapProbeDefine = $HeapProbe.compilerDefine
        if (-not $HeapProbe.known) { Write-Warning 'Unrecognized allocator SDK: using the public ESP-IDF admission fallback.' }
    } else { Write-Warning 'Allocator SDK verification unavailable: using the public ESP-IDF admission fallback.' }
    $LinkFlags = '-Wl,--wrap=esp_intr_alloc -Wl,--wrap=esp_cache_msync -Wl,--whole-archive ' + (($DsiArchives | ForEach-Object { '"' + $_ + '"' }) -join ' ') + ' -Wl,--no-whole-archive'
    # The CLI has no connected-chip auto-detection at compile time. The user's
    # esp32p4-eco2 / revision v1.0 uses prev3. Leave the IDE's default unchanged.
    $Fqbn = 'm5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,PartitionScheme=default,FlashMode=dio'
    Invoke-Arduino -CliArgs @('version')
    Invoke-Arduino -CliArgs @('core', 'list')
    Invoke-Arduino -CliArgs @('lib', 'list')
    foreach ($Sketch in @($AppName)) {
        $SketchPath = $AppPath
        $BuildPath = Join-Path $BuildEnv ('build-' + $Sketch)
        $LogPath = Join-Path $LogsDir ($Sketch + '.log')
        # Windows PowerShell 5.1 treats native stderr as ErrorRecord objects.
        # Compiler warnings on stderr must not terminate the pipeline.
        $PreviousErrorPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $IncludePath = $SketchPath.Replace('\', '/')
            $ExtraFlags = '-fstack-usage -DLV_CONF_INCLUDE_SIMPLE ' + $HeapProbeDefine + ' -I"' + $IncludePath + '"'
            & $ArduinoCli compile --clean --fqbn $Fqbn --build-path $BuildPath --library $FsLibrary --library $LvglLibrary --library $M5GfxLibrary --library $M5UnifiedLibrary --build-property ('compiler.c.extra_flags=' + $ExtraFlags) --build-property ('compiler.cpp.extra_flags=' + $ExtraFlags) --build-property ('compiler.c.elf.extra_flags=' + $LinkFlags) $SketchPath 2>&1 | Tee-Object -FilePath $LogPath
            $CompileExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $PreviousErrorPreference
        }
        if ($CompileExitCode -ne 0) { throw ('Compile failed. See ' + $LogPath) }
    }
    Write-Host 'Compilation completed. Nothing was flashed. Logs and binaries are in build-environment.'
} finally {
    foreach ($Key in $SavedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($Key, $SavedEnvironment[$Key], 'Process')
    }
}
