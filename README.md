# ts_tool
Utility for extracting strings from .ts files for translation services.

HOW TO USE:

ts_tool.exe --src v:\PROJECTS\CODIJY\ColorMagicGUI\translations\ColorMagic_EN.ts --dst t:\test\ --mode TXT

The utility take input .ts file, parse it with priority for <translation> tag (if <translation> is empty it use the <source>) and generate two files on output:

.ts file with hash's
.txt file in format [hash] "String for translation"
i.e. for example .ts:
...
<message>
<location filename="../shortcuts/shortcut_manager.cpp" line="30"></location>
<source>Arm Tool</source>
<translation>[08F5B2DC]</translation>
</message>
...

.txt:
...
[08F5B2DC] "Arm Tool"
...

Then you can send .txt file for translation.
After translation for insert translated strings back to .ts file use this command:

ts_tool.exe --src t:\test\ --dst v:\PROJECTS\CODIJY\ColorMagicGUI\translations\ColorMagic_EN.ts --mode TS

Additinal options:

--with-unfinished - for include unfinished records to result .txt file.
--with-vanished   - for include obsolete records to result .txt file. 
