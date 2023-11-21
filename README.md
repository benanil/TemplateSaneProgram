
# Template Sane C++ Program

what pushed me into this is compile times of standard library and its slow performance <br>
with debug mode<br>
## Here is how you can run test code:
to use Visual Studio:
```
cmake . -Bbuild
```
to use with gcc:
```
g++ -std=c++17 -w -O3 -mavx2 -march=native ASTL.cpp Profiler.cpp -o astl_test
```
# Windowing
ASTL supports windowing which means you can create window (only windows supported for now) <br>
it works like glfw and SDL, supports keyboard and mouse input <br>
to change the icon of the .exe <br>
```
windres icon.rc -O coff -o icon.res
```
also you can set the application information in info.rc and execute this command <br>
```
windres info.rc -O coff -o info.res <br>
```
# Contributing

feel free to contribute

#Contact
If you have any questions, feedback, or suggestions, feel free to reach out:<br>

Email: anilcangulkaya7@gmail.com<br>
Twitter: @anilcanglk12<br>
GitHub: @benanil<br>
<br>
Feel free to reach out regarding any inquiries related to the project.<br>
## Related

Here are some related and helpful links
 
 - [Raylib](https://github.com/raysan5/raylib)