cd "C:\Games\Steam\steamapps\common\Sven Co-op\svencoop\addons\metamod\dlls"

if exist MediaPlayer_old.dll (
    del MediaPlayer_old.dll
)
if exist MediaPlayer.dll (
    rename MediaPlayer.dll MediaPlayer_old.dll 
)

exit /b 0