# herb-engine-C3D

Playground for 3D rendering from scratch
I know nothing about graphics, and have used gpt to help write the x11 or winapi parts of the code,
all I want is an array of pixels, that I can update each frame with my own functions,
this is my attempt to make a 3D game like minecraft!

![screenshot](screenshot.png)

To start:
 - a fill function that takes 4 points and a colour, and fills the screen with that colour
 - a rotate and project function that takes an x y and z and rotates it about the camera, and projects it by z
 - a texture map, which takes each square, and colours it pixel by pixel from a given texture

Future ideas:
 - I have no idea how to not draw something that is behind something else...
 - add a tag for each pixel so the screen knows when we've drawn on it
 - then if a pixel has already been drawn, don't draw it again?
 - order faces by distance to camera, draw closest first?

windows:
compile with gcc -o main.exe -lgdi32 -mwindows -g .\main-windows.c
linux:
gcc -o main.o -lX11 -lm -g ./main-linux.c
