# Sube el APK de depuracion a GitHub como release de Armais/ProphetsMantle.
#
# POR QUE EXISTE ESTE FICHERO, Y EN EL REPO:
#   La primera vez esto se escribio en el scratchpad temporal de la sesion. A la
#   siguiente sesion el scratchpad ya no estaba, no habia rastro del metodo, y hubo
#   que reconstruirlo leyendo el .jsonl del transcript. Vive aqui para que eso no
#   vuelva a pasar. Si lo cambias, cambialo AQUI.
#
# COMO FUNCIONA:
#   No hay gh CLI en esta maquina y no lo hace falta. Se usa la API REST de GitHub
#   con la credencial que git ya tiene guardada para github.com (la misma con la que
#   funciona `git push`). El token NUNCA se imprime ni se escribe a disco: vive solo
#   dentro de esta invocacion de PowerShell.
#
# USO:
#   powershell -File tools\release\make_release.ps1
#   powershell -File tools\release\make_release.ps1 -NotesFile notas.md -Name "APK de prueba"
#
#   El tag es apk-<sha corto de HEAD> y el asset MANTLE-debug-<sha corto>.apk, asi que
#   dos builds del mismo commit chocan: o se borra el release anterior o se commitea.
#
# REQUISITO: el APK tiene que estar ya compilado (build_android.bat).
# NOTA: el repo es PRIVADO. El release NO publica el binario; para descargarlo hay
#   que abrir el enlace con la sesion de GitHub iniciada, si no da 404.

param(
  [string]$NotesFile = '',
  [string]$Name      = '',
  [string]$Apk       = ''
)

$ErrorActionPreference = 'Stop'
$repoDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Apk) { $Apk = Join-Path $repoDir 'android\app\build\outputs\apk\debug\app-debug.apk' }
$owner = 'Armais'
$repo  = 'ProphetsMantle'

if (-not (Test-Path $Apk)) { Write-Output "[ERROR] no hay APK en $Apk (compila con build_android.bat)"; exit 1 }

$sha   = (git -C $repoDir rev-parse HEAD).Trim()
$short = $sha.Substring(0,7)
$tag   = "apk-$short"
if (-not $Name) { $Name = "APK de depuracion $short" }

$notes = if ($NotesFile -and (Test-Path $NotesFile)) { Get-Content $NotesFile -Raw } else {
@"
Build de **depuracion** (arm64-v8a) del commit ``$short``, para probar en el movil.
No esta firmado para distribucion: hay que permitir "instalar apps de origen desconocido".
"@
}

# --- credencial: la que git ya tiene para github.com --------------------------
$cred  = "protocol=https`nhost=github.com`n`n" | git credential fill 2>$null
$token = ($cred | Where-Object { $_ -like 'password=*' }) -replace '^password=',''
$user  = ($cred | Where-Object { $_ -like 'username=*' }) -replace '^username=',''
if (-not $token) { Write-Output '[ERROR] git no tiene credencial guardada para github.com'; exit 1 }
Write-Output ("credencial: usuario {0}, token de {1} caracteres" -f $user, $token.Length)

$h = @{ Authorization = "Bearer $token"; 'User-Agent' = 'mantle-release'; Accept = 'application/vnd.github+json' }

$info = Invoke-RestMethod -Uri "https://api.github.com/repos/$owner/$repo" -Headers $h
Write-Output ("repo: {0}  visibilidad={1}" -f $info.full_name, $info.visibility)

# --- release ------------------------------------------------------------------
$body = @{
  tag_name         = $tag
  target_commitish = $sha
  name             = $Name
  prerelease       = $true
  body             = $notes
} | ConvertTo-Json -Depth 4

try {
  $rel = Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/$owner/$repo/releases" -Headers $h -Body $body -ContentType 'application/json'
} catch {
  Write-Output ("[ERROR] creando el release: {0}" -f $_.Exception.Message)
  if ($_.ErrorDetails) { Write-Output $_.ErrorDetails.Message }
  exit 1
}
Write-Output ("release creado: {0}" -f $rel.html_url)

# --- subir el APK como asset ---------------------------------------------------
$upload = ($rel.upload_url -replace '\{\?name,label\}','') + "?name=MANTLE-debug-$short.apk"
Write-Output ("subiendo el APK ({0:N1} MB)..." -f ((Get-Item $Apk).Length/1MB))
try {
  $asset = Invoke-RestMethod -Method Post -Uri $upload -Headers $h -InFile $Apk -ContentType 'application/vnd.android.package-archive'
} catch {
  Write-Output ("[ERROR] subiendo el APK: {0}" -f $_.Exception.Message)
  if ($_.ErrorDetails) { Write-Output $_.ErrorDetails.Message }
  exit 1
}
Write-Output ("[OK] adjunto: {0}  ({1:N1} MB)" -f $asset.name, ($asset.size/1MB))
Write-Output ("DESCARGA: {0}" -f $asset.browser_download_url)
Write-Output ("RELEASE : {0}" -f $rel.html_url)
