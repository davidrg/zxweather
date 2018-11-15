@echo off
cls

REM ----------------------------------------------------------------------------------------------------------------------------
REM zxweather desktop client automatic distribution upload script.
REM ============================================================================================================================
REM This script will:
REM   1) Run the build script (BUILD_SCRIPT) if no dist directory exists
REM   2) Creates a temporary copy of the dist directory and adds it to a zip file
REM   3) Uploads the zip file to a temporary file, deletes the file on the server then renames the new file to the old filename
REM   4) Updates the intro page with todays date and the current known issues list
REM 
REM The end result will be a new build (or the pre-built one in the dist directory) on the server and an updated intro page.
REM
REM This relies on SSH keys being loaded in pagent! Plus plink and pscp in the path!
REM ----------------------------------------------------------------------------------------------------------------------------


set INPUT_DIR=dist
set OUTPUT_DIR=zxweather_desktop
set KNOWN_ISSUES=known_issues.html

set DEST_HOST=david@fremont.zx.net.nz
set DEST_DIR=/opt/zxw_static/sb/desktop
set HTML_FILE=%DEST_DIR%/intro.html

for /f "tokens=1-4 delims=/ " %%i in ("%date%") do (
     set dow=%%i
     set day=%%j
     set month=%%k
     set year=%%l
)
set mon=JAN

if "%month%"=="02" (
    set mon=FEB
)
if "%month%"=="03" (
    set mon=MAR
)
if "%month%"=="04" (
    set mon=APR
)
if "%month%"=="05" (
    set mon=MAY
)
if "%month%"=="06" (
    set mon=JUN
)
if "%month%"=="07" (
    set mon=JUL
)
if "%month%"=="08" (
    set mon=AUG
)
if "%month%"=="09" (
    set mon=SEP
)
if "%month%"=="10" (
    set mon=OCT
)
if "%month%"=="11" (
    set mon=NOV
)
if "%month%"=="12" (
    set mon=DEC
)


REM Update release date and known issues
REM ---------------------------------------------------
echo Uploading known issues...
set DEST_KNOWN_ISSUES=%DEST_DIR%/known_issues.tmp
pscp %KNOWN_ISSUES% %DEST_HOST%:%DEST_KNOWN_ISSUES%
echo.

echo Updating intro page with release date and known issues...
plink %DEST_HOST% "sed -E 's/(<\!\-\-RELEASE_DATE\-\->)(.*)(<\!\-\-END_RELEASE_DATE\-\->)/\1%day%\-%mon%\-%year%\3/' %HTML_FILE% > %DEST_DIR%/out.html; sed -n '/<\!\-\-KNOWN_ISSUES\-\->/{p;:a;N;/<\!\-\-END_KNOWN_ISSUES\-\->/!ba;s/.*\n/KNOWN_ISSUES_PLACE_HOLDER\n/};p' %DEST_DIR%/out.html > %DEST_DIR%/out2.html; awk -v RS='\0' 'NR==FNR{r=$0;next}{sub(/KNOWN_ISSUES_PLACE_HOLDER/,r)}7' %DEST_DIR%/known_issues.tmp %DEST_DIR%/out2.html > %HTML_FILE%; rm %DEST_DIR%/out.html; rm %DEST_DIR%/out2.html; rm %DEST_KNOWN_ISSUES%"
echo.

echo Distribution upload complete.
pause