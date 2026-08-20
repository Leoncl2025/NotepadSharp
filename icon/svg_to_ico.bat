@echo off
set ICON_SIZES=16 32 48 64 128 256

for %%s in (%ICON_SIZES%) do (
    echo Exporting %%sx%%s PNG...
    "C:\Program Files\Inkscape\bin\inkscape.exe" NotepadSharp.svg -w %%s -h %%s --export-type=png --export-filename=ns%%s.png
)

".\ImageMagick\magick" convert -background transparent ns16.png ns32.png ns48.png ns64.png ns128.png ns256.png NotepadSharp.ico

for %%s in (%ICON_SIZES%) do del ns%%s.png

"C:\Program Files\Inkscape\bin\inkscape.exe" NotepadSharp.svg --export-type=png --export-dpi=96 --export-background-opacity=0
copy NotepadSharp.png ..\src\icons\NotepadSharp.png
