# ============================================================
#  MCPE 0.4.0 Android Build Script  —  from-scratch capable
#  Works on a clean machine; creates all dirs, keystore and
#  stub Java files automatically.
#
#  Usage:
#    .\build.ps1          # full build (NDK + Java + APK + install)
#    .\build.ps1 -NoCpp   # skip NDK rebuild (Java/assets changed)
#    .\build.ps1 -NoJava  # skip Java recompile (C++ changed only)
#    .\build.ps1 -NoBuild # repackage + install only (no recompile)
# ============================================================
param(
    [switch]$NoCpp,
    [switch]$NoJava,
    [switch]$NoBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Paths ────────────────────────────────────────────────────
$repo       = $PSScriptRoot
$apkbuild   = "C:\apkbuild"
$ndk        = "C:\android-ndk-r14b"
$sdkTools   = "$env:LOCALAPPDATA\Android\Sdk\build-tools\35.0.0"
$androidJar = "$env:LOCALAPPDATA\Android\Sdk\platforms\android-36\android.jar"
$adb        = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
$keystore   = "$apkbuild\debug.keystore"
$pkg        = "com.mojang.minecraftpe"

# Auto-detect keytool from JAVA_HOME, then from common install locations
$keytool = if ($env:JAVA_HOME) { "$env:JAVA_HOME\bin\keytool.exe" } else {
    $found = Get-ChildItem "C:\Program Files\Java","C:\Program Files\Eclipse Adoptium" `
        -Filter keytool.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $found) { throw "keytool not found. Set JAVA_HOME or install a JDK." }
    $found
}

$jniDir     = "$repo\project\android\jni"
$libSrc     = "$repo\project\android\libs\arm64-v8a\libminecraftpe.so"
$libDst     = "$apkbuild\lib\arm64-v8a\libminecraftpe.so"
$manifest   = "$repo\project\android_java\AndroidManifest.xml"
$res        = "$repo\project\android_java\res"
$javaSrc    = "$repo\project\android_java\src"
$stubsDir   = "$apkbuild\stubs"
$rJava      = "$apkbuild\gen\R.java"
$classesDir = "$apkbuild\classes"
$dexOut     = "$apkbuild\classes.dex"
$dataDir    = "$repo\data"

$unsigned   = "$apkbuild\minecraftpe-unsigned.apk"
$aligned    = "$apkbuild\minecraftpe-aligned.apk"
$signed     = "$apkbuild\minecraftpe-debug.apk"

Add-Type -Assembly "System.IO.Compression.FileSystem"

function Write-Step([string]$msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Assert-ExitCode([string]$step) {
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $step (exit $LASTEXITCODE)" -ForegroundColor Red; exit 1 }
}
function New-Dir([string]$path) { New-Item $path -ItemType Directory -Force | Out-Null }
function Write-Stub([string]$rel, [string]$content) {
    $full = "$stubsDir\$rel"
    New-Dir (Split-Path $full -Parent)
    if (-not (Test-Path $full)) { [System.IO.File]::WriteAllText($full, $content); Write-Host "  stub: $rel" }
}

# ── 0. Bootstrap ─────────────────────────────────────────────
Write-Step "Bootstrap"

New-Dir $apkbuild
New-Dir "$apkbuild\lib\arm64-v8a"
New-Dir "$apkbuild\gen"
New-Dir $stubsDir

if (-not (Test-Path $keystore)) {
    Write-Host "  generating debug.keystore..."
    $eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    & $keytool -genkeypair `
        -keystore $keystore -storepass android -keypass android `
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 `
        -dname "CN=Android Debug,O=Android,C=US" 2>&1 | Out-Null
    $ErrorActionPreference = $eap
    Assert-ExitCode "keytool"
    Write-Host "  keystore created"
} else { Write-Host "  keystore OK" }

# Java helper classes formerly generated here now live in project/android_java/src

# ── 1. NDK build ─────────────────────────────────────────────
if (-not $NoCpp -and -not $NoBuild) {
    Write-Step "NDK build (arm64-v8a)"
    # NDK r14b on Windows hits the 32K CreateProcess limit with long paths.
    # Work around it by building through a short junction C:\m -> repo root.
    $junctionBase = "C:\m"
    if (-not (Test-Path $junctionBase)) {
        & cmd.exe /c "mklink /J `"$junctionBase`" `"$repo`"" | Out-Null
    }
    Push-Location "$junctionBase\project\android\jni"
    $env:NDK_MODULE_PATH = "$junctionBase\project\lib_projects"
    # run ndk-build and capture everything; let user see full output for debugging
    $ndkOutput = & "$ndk\ndk-build.cmd" NDK_PROJECT_PATH="$junctionBase\project\android" APP_BUILD_SCRIPT="$junctionBase\project\android\jni\Android.mk" 2>&1 | Tee-Object -Variable ndkOutput
    # dump entire output for diagnosis
    Write-Host "---- NDK BUILD OUTPUT BEGIN ----"
    $ndkOutput | ForEach-Object { Write-Host $_ }
    Write-Host "---- NDK BUILD OUTPUT END ----"
    # optionally highlight errors/warnings afterwards
    $ndkOutput | Where-Object { $_ -match "error:|warning:|libminecraftpe|In file included" }
    Pop-Location
    Assert-ExitCode "ndk-build"
    Copy-Item $libSrc $libDst -Force
    Write-Host "  .so  ->  $libDst"
}

# ── 2. Java compile ──────────────────────────────────────────
if (-not $NoJava -and -not $NoBuild) {
    Write-Step "Java compile"

    New-Dir (Split-Path $rJava -Parent)
    & "$sdkTools\aapt.exe" package -f -M $manifest -S $res -I $androidJar -J "$apkbuild\gen" -F "$apkbuild\_rgen.apk" 2>&1 | Out-Null
    Assert-ExitCode "aapt R.java"
    Remove-Item "$apkbuild\_rgen.apk" -ea SilentlyContinue

    $srcs = @(
        Get-ChildItem $javaSrc  -Recurse -Filter "*.java" | Select-Object -Exp FullName
        $rJava
    )

    Remove-Item $classesDir -Recurse -Force -ea SilentlyContinue
    New-Dir $classesDir
    $eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $errors = & javac --release 8 -cp $androidJar -d $classesDir @srcs 2>&1 |
              Where-Object { $_ -match "error:" }
    $ErrorActionPreference = $eap
    if ($errors) { Write-Host $errors -ForegroundColor Red; exit 1 }
    Write-Host "  javac OK"

    $classFiles = Get-ChildItem $classesDir -Recurse -Filter "*.class" | Select-Object -Exp FullName
    & "$sdkTools\d8.bat" --min-api 21 --output $apkbuild $classFiles
    Assert-ExitCode "d8"
    Write-Host "  d8  ->  $dexOut"
}

# ── 3. Package APK ───────────────────────────────────────────
Write-Step "Package APK"
Remove-Item $unsigned,$aligned,$signed -ea SilentlyContinue

& "$sdkTools\aapt.exe" package -f -M $manifest -S $res -I $androidJar -F $unsigned
Assert-ExitCode "aapt package"

$zip = [System.IO.Compression.ZipFile]::Open($unsigned, 'Update')
try {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$dexOut,"classes.dex",[System.IO.Compression.CompressionLevel]::Fastest)|Out-Null
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$libDst,"lib/arm64-v8a/libminecraftpe.so",[System.IO.Compression.CompressionLevel]::NoCompression)|Out-Null
    Get-ChildItem $dataDir -Recurse -File | ForEach-Object {
        $rel=$_.FullName.Substring("$dataDir\".Length).Replace('\','/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$_.FullName,"assets/$rel",[System.IO.Compression.CompressionLevel]::NoCompression)|Out-Null
    }
} finally { $zip.Dispose() }
Write-Host "  APK assembled"

& "$sdkTools\zipalign.exe" -p 4 $unsigned $aligned; Assert-ExitCode "zipalign"
& "$sdkTools\apksigner.bat" sign --ks $keystore --ks-pass pass:android --key-pass pass:android --out $signed $aligned; Assert-ExitCode "apksigner"
Write-Host "  signed  ->  $signed"

# ── 4. Install ───────────────────────────────────────────────
Write-Step "Install"
& $adb shell am force-stop $pkg
& $adb uninstall $pkg 2>$null
& $adb install --no-incremental $signed
Assert-ExitCode "adb install"

Write-Host "`nDone." -ForegroundColor Green