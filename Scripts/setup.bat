pushd ..
REM delete generator if you want to use visual studio 
cmake -S . -O build -G "MinGW Makefiles"
