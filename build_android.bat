@echo off
setlocal
REM ============================================================================
REM MANTLE - APK de depuracion para Android (arm64-v8a)
REM
REM   build_android.bat            compila el APK
REM   build_android.bat install    compila, instala por adb y lanza el juego
REM
REM Salida: android\app\build\outputs\apk\debug\app-debug.apk
REM ============================================================================

REM JAVA_HOME del sistema apunta a un JDK de Unity que ya no esta instalado, y no hay
REM `java` en el PATH: se usa el JBR (JDK 21) que trae Android Studio.
set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
set "PATH=%ANDROID_HOME%\platform-tools;%JAVA_HOME%\bin;%PATH%"

if not exist "%JAVA_HOME%\bin\java.exe" (
    echo [ERROR] No hay JDK en "%JAVA_HOME%".
    echo         Instala Android Studio o apunta JAVA_HOME a un JDK 17+.
    exit /b 1
)
if not exist "%ANDROID_HOME%\ndk\28.2.13676358" (
    echo [ERROR] Falta el NDK 28.2.13676358 en "%ANDROID_HOME%\ndk".
    echo         Android Studio ^> SDK Manager ^> SDK Tools ^> NDK ^(Side by side^).
    exit /b 1
)
if not exist "%ANDROID_HOME%\cmake\3.22.1" (
    echo [ERROR] Falta CMake 3.22.1 en "%ANDROID_HOME%\cmake".
    echo         Android Studio ^> SDK Manager ^> SDK Tools ^> CMake.
    exit /b 1
)

pushd "%~dp0android"
call "%~dp0android\gradlew.bat" assembleDebug
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" (
    echo [ERROR] Gradle fallo con codigo %RC%.
    exit /b %RC%
)

set "APK=%~dp0android\app\build\outputs\apk\debug\app-debug.apk"
echo.
echo APK: %APK%
echo.

if /I not "%~1"=="install" (
    echo Para probarlo en el movil:
    echo   adb install -r "%APK%"
    echo   adb shell am start -n com.mantle.prophet/org.libsdl.app.SDLActivity
    echo   adb logcat -s SDL:V SDL/APP:V SDL/ERROR:V AndroidRuntime:E DEBUG:F
    goto :eof
)

echo Instalando...
adb install -r "%APK%" || exit /b 1
adb shell am start -n com.mantle.prophet/org.libsdl.app.SDLActivity
echo.
echo El log del motor sale con la etiqueta SDL/APP, no MANTLE:
echo   adb logcat -s SDL:V SDL/APP:V SDL/ERROR:V AndroidRuntime:E DEBUG:F
