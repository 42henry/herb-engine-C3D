# herb-engine-C3D

Playground for 3D rendering from scratch!  
I know nothing about graphics, all I want is an array of pixels,
that I can update each frame with my own functions,
this is my attempt to make a 3D game like minecraft!
I have used gpt to help write the x11 or winapi parts of the code,
so I can just have my array of pixels that I update each frame.
I also used gpt to make the perlin noise function, as the maths for this is outside project scope,
but I thought it would be nice to have. Everything else is from scratch!

Work done so far:
 - a fill function that takes 4 points and a colour, and fills it in
 - a rotate and project function that takes an x y and z and rotates it about the camera using a global x and y rotation, and projects it by z
 - texture mapping, which takes each square, and colours it pixel by pixel from a given texture
 - simple cubicy collisions
 - simple hotbar/hand with different blocks accesible by pressing 1-6
 - back face culling, neighbour face culling
 - placing and removing cubes within any chunk
 - chunk system with dynamically saved chunk edits
 - chunk generation from a given noise function
 - tree/structure generation saved using the chunk edits
 - fog at chunk borders
 - day night cycle
 - transparent water blocks that don't have collisions

Known issues:
 - collision issues when passing chunk borders
 - rendering issues when some points of a square have negative z values

Performance:
 - from profiling I can see that the fill square function accounts for nearly half the total program instructions
 - cache hits seem good, so the next step would be to come up with a faster fill square algorithm...
 - I tried a few similar variations with fewer conditionals, but with little difference so I'm happy with this.

Next steps:
 - I've come up with a way to make the sunlight better:
   - store the sun as a point on a semicircle centred around a cube, with a large enough radius
   - as the point moves around, calculate the distance from each face of the cube, to the point
   - the difference between the closest face, and the furthest face represents a scale from completely dark, to completely bright
   - use that scale to colour each face a certain brightness based on their distance to the sun
 - I'd also love to try use sockets to make multiplayer from scratch - or just use enet...
 - biomes sounds like a tough nut to crack so I'd love to try that some time too!

To compile:
 - linux:  
   - gcc -o main.o -lX11 -lm -O3 -march=native ./linux2.c  
 - Windows:
   - gcc -o main.exe -lgdi32 -mwindows -O3 -march=native .\main-windows.c

Or play right now (claude rewrote linux_platform.c for compilation with emcc to web assembly!) on my website:  
https://niceboisnice.com/digmake
  
Dev screenshots:

![screenshot](pics/screenshot0.png)
![screenshot](pics/screenshot.png)
![screenshot](pics/screenshot2.png)
![screenshot](pics/screenshot3.png)
![screenshot](pics/screenshot4.png)
![screenshot](pics/screenshot5.png)
![screenshot](pics/screenshot6.png)
![screenshot](pics/screenshot7.png)
![screenshot](pics/screenshot8.png)
![screenshot](pics/screenshot9.png)
![screenshot](pics/screenshot10.png)
![screenshot](pics/screenshot11.png)
![screenshot](pics/screenshot12.png)
![screenshot](pics/screenshot13.png)
![screenshot](pics/screenshot14.png)

