@Echo Off
REG ADD "HKCR\cccaster" /f
REG ADD "HKCR\cccaster" /ve /t REG_SZ /d "URL:cccaster Protocol" /f
REG ADD "HKCR\cccaster" /v "URL Protocol" /t REG_SZ /d "" /f
REG ADD "HKCR\cccaster\shell\open\command" /ve /t REG_SZ /d "\"%~dp0cccaster.v2.1.exe\" \"%%1\"" /f
PAUSE