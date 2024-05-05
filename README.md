
# Template Sane C++ Program
This is an engine for making applications and games for Android and Windows platforms.<br>
I've tried to make this software as less as third party software possible.<br>
Only used third party libraries are header only libraries such as stb.<br>
I haven't used object orianted programming (maybe in feature), <br>
code is written in a way that has: fast compile times and faster debug performance. <br>
SDL, glfw or glm is not used so we have full control over software, [ASTL](https://github.com/ufbx/ufbx) used instead of C++ stl<br><br>
Main file of the project is SaneProgram.cpp there are 4 functions in this file:<br>
AXInit()  -> runs before window initialization, you can set window name, position, size or VSync here.<br>
AXStart() -> runs after initialization of opengl, you can import your assets here.<br>
AXLoop()  -> runs every frame you can add game logic and renderer code here.<br>
AXExit()  -> destroy everything before closing the engine.<br>
PlatformAndroid.cpp or PlatformWindows.cpp will find and call these functions.<br>
whithout these functions you will get error. 

## Here is how you can run test code:
to use with Visual Studio:
```
cmake . -Bbuild
```
after that you can build the project by running build.bat or built within Visual Studio.<br>
you can also build with GCC or Clang, uncomment GCC or Clang lines in build.bat and run the script <br>

# Windowing
Template program supports windowing which means you can create window in windows and android platforms <br>
it works like glfw and SDL, supports keyboard, mouse and touch input <br>
to change the icon of the .exe and application information (CompanyName, Version, Product name)<br>
edit info.rc and execute this command <br>
```
windres SaneProgram.rc -O coff -o SaneProgram.res
```
To change icon and information of the .exe: <br>
Visual Studio go to SolutionExplorer-> right click SaneProgram->AddExisting->SaneProgram.res

# Good to Know
'r' prefix used for renderer functions
'w' prefix used for windowing functions
this is doom like thing


# Building Android Project
You have to move Meshes and Shaders folders to TemplateSaneProgram\Android\app\src\main\assets folder <br>
but you can use python script manually. This command below moves shaders meshes and texture files(astc) <br>
```
python3 AndroidBuild.py move_assets
```
to build apk you can run the command below<br>
```
python3 AndroidBuild.py build
```
to run the project on your phone you have to run this command below.<br>
conntect your phone to your pc and make sure usb debugging is on
```
python3 AndroidBuild.py run 
```
<br>
or you can use Android Studio to build and run your project, just open Android Folder with Android studio

# Other Info
Blender Mixamo Character Import Settings: 
    Rotation quaternion:  w:1.0, xyz: 0.0
    Scale.xyz          :  0.140, 0.140, 0.140 
    Transform.+yUp     :  uncheck
    
    Blender Multi Animation: https://shorturl.at/ajszQ

# Used Libraries
for fbx scenes [ufbx](https://github.com/ufbx/ufbx)<br>
importing, resizing, saving textures + importing font  [stb libraries](https://github.com/nothings/stb)<br>
astc texture format codec for android textures [astc-encoder](https://github.com/ARM-software/astc-encoder)<br>
fast compression library by facebook [zstandard](https://github.com/facebook/zstd)<br>
for dxt texture compression [etcpak](https://github.com/wolfpld/etcpak)<br>

# Contributing
feel free to contribute

# Contact
If you have any questions, feedback, or suggestions, feel free to reach out:<br>

Email: anilcangulkaya7@gmail.com<br>
Twitter: @anilcanglk12<br>
GitHub: @benanil<br>