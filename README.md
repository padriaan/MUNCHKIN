# MUNCHKIN
Remake of 38: Odyssey 2 K.C. Munchkin / Philips Videopac Munchkin
==================================================================

Remake of 38: Odyssey 2 K.C. Munchkin / Philips Videopac Munchkin
Originally released in 1982, programmed by Ed Averett.  



Created with SDL 1.2 in C.          
Requirements (devel + libs):
- SDL 1.2 
- SDL_mixer
- SDL_ttf
- Images (bmp), sounds (wav), Font (o2.ttf modified)

Created by Peter Adriaanse may 2025.
- Version 1.0  SDL 1.2 (Linux + Windows)

Press 1, Ctrl or Fire to start game.

Controls:  
- Joystick or cursor keys
- Use 8 to toggle full-screen on/off.  
- Esc to quit from game. Esc in start-screen to quit all.  
- Character keys for entering high score name. Return to complete.



Compile and link from source
-----------------------------
First install a SDL 1.2 development environment and C-compiler.

Linux:  
$ gcc -o munchkin munchkin.c -I/usr/include/SDL -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf -lm

Windows (using MinGW):  
gcc -o munchkin.exe munchkin.c -Lc:\MinGW\include\SDL  -lmingw32 -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf

Run binary
------------
Download src and data folders. Extract data.zip (to get data/images and data/sounds).

Execute in Windows:   
double-click munchkin.exe

Execute in Linux:   
$ export LD_LIBRARY_PATH=<folder where munchkin/src/linux_libs is located>  
$ cd src  
$ chmod 644 munchkin
$ ./munchkin
# MUNCHKIN
