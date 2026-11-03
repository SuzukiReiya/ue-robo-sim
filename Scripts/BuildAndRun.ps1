#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:ROBOT_SIM_UE_ROOT,
    [switch]$BuildOnly,
    [ValidateRange(0,1000)][int]$CommandDelayMs = 100,
    [ValidateRange(0,1000)][int]$VideoDelayMs = 100,
    [ValidateRange(0,200)][int]$JitterMs = 0,
    [ValidateRange(0,1)][double]$LossRate = 0,
    [ValidateRange(0,1)][double]$SharedWeight = 0.5,
    [int]$Seed = 12345,
    [switch]$ExportFrames
)
$ErrorActionPreference = 'Stop'
try {
    if ($env:OS -ne 'Windows_NT') { throw 'Windows上のPowerShellで実行してください。' }
    if (-not $EngineRoot) { $EngineRoot = 'C:\Program Files\Epic Games\UE_5.6' }
    $EngineRoot = (Resolve-Path $EngineRoot).Path
    $Version = Get-Content (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
    if ($Version.MajorVersion -ne 5 -or $Version.MinorVersion -ne 6) { throw 'UE 5.6.xを指定してください。' }
    $Root = Split-Path $PSScriptRoot -Parent
    $Project = Join-Path $Root 'RobotSim.uproject'
    $Logs = Join-Path $Root '.local\logs'
    New-Item -ItemType Directory -Force $Logs | Out-Null
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') RobotSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object (Join-Path $Logs 'build.log')
    if ($LASTEXITCODE -ne 0) { throw 'UEのビルドに失敗しました。build.logを確認してください。' }
    & (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $Project '-run=pythonscript' "-script=$PSScriptRoot\CreateMap.py" -unattended -nop4 -NullRHI -stdout -UTF8Output 2>&1 | Tee-Object (Join-Path $Logs 'map.log')
    if ($LASTEXITCODE -ne 0) { throw 'マップ生成に失敗しました。map.logを確認してください。' }
    if (-not $BuildOnly) {
        $Invariant = [Globalization.CultureInfo]::InvariantCulture
        $RunArgs = @($Project, '/Game/Generated/Minimal', '-game', '-windowed', '-ResX=1280', '-ResY=720', '-d3d11', '-stdout', '-FullStdOutLogOutput', "-JitterMs=$JitterMs", "-LossRate=$($LossRate.ToString($Invariant))", "-SharedWeight=$($SharedWeight.ToString($Invariant))", "-Seed=$Seed")
        # ラグを明示した場合だけ起動時に上書き。省略時はSaved/Configの設定を優先する。
        if ($PSBoundParameters.ContainsKey('CommandDelayMs')) {
            $RunArgs += "-CommandDelayMs=$CommandDelayMs"
        }
        if ($PSBoundParameters.ContainsKey('VideoDelayMs')) {
            $RunArgs += "-VideoDelayMs=$VideoDelayMs"
        }
        if ($ExportFrames) { $RunArgs += '-ExportFrames' }
        & (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') @RunArgs 2>&1 | Tee-Object (Join-Path $Logs 'run.log')
        if ($LASTEXITCODE -ne 0) { throw '起動中にエラーが発生しました。run.logを確認してください。' }
    }
} catch {
    Write-Error $_
    exit 1
}
