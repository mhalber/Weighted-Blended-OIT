# Weighted, Blended OIT
<p align="center">
<img src='imgs/oit.png'>
</p>

A simple, single-file, OpenGL implementation of 


["Weighted Blended Order-Independent Transparency"](http://jcgt.org/published/0002/02/09/paper.pdf) 

by Morgan McGuire and Louis Bavoil.


This small demo allows the user to switch between OIT rendering and the fixed order rendering - rotating the scene around allows one to appreciate the order-independent nature of the implemented technique. 

It is also possible to visualize different textures generated during the OIT rendering.

Lastly, by default, the transparency shader uses Equation 9 from the original paper, but one can also edit source code to switch to exponential function with coefficients fit to best resemble the fixed order blend at the initial camera distance. Check the 'transparency_fs_src' variable that stores the respective shader code.

## Building

The project contains a cmake file - depending on your platform you might want to use cmake-gui, or command line,
In a command line environment with GNU make, you can build the project as:

~~~
mkdir build
cd build
cmake ..
make
~~~

## Keybindings

`space` - switch between OIT and fixed order rendering

`1` - if showing OIT, switch to showing the final framebuffer

`2` - if showing OIT, switch to showing the accumulation texture

`3` - if showing OIT, switch to showing the revelage texture

## Camera controls

`lmb` - rotate camera

`rmb` - pan camera

`mouse scroll` - zoom camera 
