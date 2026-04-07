tree /f /a > tree.txt

cd Build/Scripts

make -f "Makefile.Injector.mak" all
if %ERRORLEVEL% NEQ 0 (echo ERROR:%ERRORLEVEL% && goto errEnd)

make -f "Makefile.LoadMe.mak" all
if %ERRORLEVEL% NEQ 0 (echo ERROR:%ERRORLEVEL% && goto errEnd)

make -f "Payloads/Makefile.ExampleDLL.mak" all
if %ERRORLEVEL% NEQ 0 (echo ERROR:%ERRORLEVEL% && goto errEnd)

make -f "Payloads/Makefile.Keylogger.mak" all
if %ERRORLEVEL% NEQ 0 (echo ERROR:%ERRORLEVEL% && goto errEnd)

:errEnd
exit