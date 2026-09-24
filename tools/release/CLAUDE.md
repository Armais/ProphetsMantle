# `tools/release/` - mandar el APK al movil

## Subir el APK a GitHub: `make_release.ps1`

**Esto ya esta resuelto. No lo vuelvas a investigar ni digas que no se puede.**

```powershell
& tools\release\make_release.ps1
```

**Lanzalo con pwsh 7, no con `powershell -File`.** La Windows PowerShell 5.1 que abre
`powershell -File` canaliza distinto el string que se le pasa a `git credential fill` y
muere con `fatal: refusing to work with credential missing protocol field`. La herramienta
PowerShell de Claude Code ya es pwsh 7: llamarlo con `&` y la ruta basta.

Crea un **prerelease** en `Armais/ProphetsMantle` con tag `apk-<sha corto de HEAD>` y le
adjunta `android\app\build\outputs\apk\debug\app-debug.apk` como
`MANTLE-debug-<sha corto>.apk`. Imprime las dos URLs (`RELEASE:` y `DESCARGA:`).

Lo que hay que saber antes de tocarlo:

- **No hay `gh` CLI en esta maquina y no hace falta.** El script habla con la API REST
  y saca el token de `git credential fill` para `github.com`, que es la misma credencial
  con la que funciona `git push`. El token no se imprime ni se guarda.
- **El repo es PRIVADO**: el release no publica el binario a nadie. Para descargarlo hay
  que abrir el enlace en el movil con la sesion de GitHub iniciada; si no, da 404.
- **`*.apk` esta en `.gitignore` a proposito.** El APK son 68 MB (crecio con la musica de 2026-09-14): va como asset de un
  release, nunca commiteado al repo.
- El tag lleva el sha de HEAD, asi que **dos builds del mismo commit chocan** (422 desde
  la API). O se borra el release anterior o se commitea antes de volver a subir.
- Compila primero con `build_android.bat`; el script solo sube, no compila.

## Para alguien de fuera: `Armais/mantle-builds` (PUBLICO)

El repo del codigo es privado, asi que su enlace de descarga da 404 a quien no tenga
acceso al repositorio, y la API de colaboradores **pide nombre de usuario, no email**:
a alguien de quien solo sabes el correo no lo puedes invitar desde aqui.

Para eso esta `Armais/mantle-builds`, un repo **publico y vacio** (solo su README) que no
contiene ni una linea del juego: solo releases con el APK. El enlace de descarga funciona
sin cuenta de GitHub.

```powershell
& tools\release\make_release.ps1 -Repo 'mantle-builds' -CreatePublic -Name "APK de prueba"
```

`-CreatePublic` solo hace falta la primera vez (crea el repo). Ojo con lo que implica: ese
APK es **descargable por cualquiera** que tenga el enlace.

En ese repo el tag NO se ancla al commit del codigo (ese sha no existe alli y la API
responde 422): el script omite `target_commitish` cuando el destino no es `ProphetsMantle`.

## Por Telegram no cabe

El APK pesa 68 MB y el limite de un bot de Telegram son 50. No hay nada que apretar: 46
de esos MB son los PNG cinematicos, que ya vienen comprimidos. Por Telegram va **el
enlace** del release, con la skill `enviar-por-telegram`.
