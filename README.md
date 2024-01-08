
# Template Sane C++ Program

what pushed me into this is compile times of standard library and its slow performance <br>
with debug mode<br>
## Here is how you can run test code:
to use Visual Studio:
```
cmake . -Bbuild
```
build with with gcc, by running build.bat
# Windowing
Template program supports windowing which means you can create window in windows and android platforms <br>
it works like glfw and SDL, supports keyboard, mouse and touch input <br>
to change the icon of the .exe edit info.rc <br>
also application information(CompanyName, Version, Product name) in info.rc and execute this command <br>
don't forget to build resource file with this command <br>
```
windres SaneProgram.rc -O coff -o SaneProgram.res
```
To change icon and information of the .exe: <br>
Visual Studio go to SolutionExplorer-> right click SaneProgram->AddExisting->SaneProgram.res
# Contributing

feel free to contribute

# Contact
If you have any questions, feedback, or suggestions, feel free to reach out:<br>

Email: anilcangulkaya7@gmail.com<br>
Twitter: @anilcanglk12<br>
GitHub: @benanil<br>

# Building Android Project
copy Meshes, Shaders and Textures folders to TemplateSaneProgram/Android/app/src/main/assets folder <br>
and open TemplateSaneProgram/Android folder with android studio hit build and run.
this might be automated in the feature

# Used Libraries
for fbx scenes [ufbx](https://github.com/ufbx/ufbx)<br>
importing and resizing textures [stb libraries](https://github.com/nothings/stb)<br>
astc texture format codec for android textures [astc-encoder](https://github.com/ARM-software/astc-encoder)<br>
fast compression library by facebook [zstandard](https://github.com/facebook/zstd)<br>
for dxt texture compression [etcpak](https://github.com/wolfpld/etcpak)<br>