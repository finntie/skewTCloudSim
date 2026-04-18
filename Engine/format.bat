@echo off
FOR /R "source" %%G IN (*.cpp *.hpp) DO (
    echo Bzzz bzzz! Formatting %%G...
    clang-format -i "%%G"
    echo.
)
FOR /R "include" %%G IN (*.cpp *.hpp) DO (
    echo Bzzz bzzz! Formatting %%G...
    clang-format -i "%%G"
    echo.
)
echo All formatted!

