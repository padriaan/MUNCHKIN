/*******************************************************************************************
munchkin.c 

Remake of Philips Videopac    38 Munchkin
          Magnavox Osyssey 2  38 KC Munchkin
          Originally released in 198xxx, programmed by Ed Averett.

Created with SDL 1.2 in C.          
Requirements (devel + libs):
- SDL 1.2 
- SDL_mixer
- SDL_ttf
- Images (bmp), sounds (wav), Font (o2.ttf modified)

Created by Peter Adriaanse May 2025.
Version 1.0 FULL SCREEN version

Choose start option, or press 1, Ctrl or joytick fire.

Controls: Joystick or cursor keys
          Use 8 to toggle full-screen on/off (or use -f at command line
          Esc to quit from game. Esc in start-screen to quit all.
          Character keys for entering high score name. Return to complete.

Command line option: -fullscreen or -f to start in fullscreen.

Compile and link in Linux:
$ gcc -o munchkin munchkin.c -I/usr/include/SDL -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf -lm

Windows (using MinGW):
gcc -o munchkin.exe munchkin.c -Lc:\MinGW\include\SDL  -lmingw32 -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf

***********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_ttf.h>
#endif

#ifndef _WIN32
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#endif

/* constants */
#define DATA_PREFIX "../data/"
#define NUM_IMAGES 148

#define VERT_LINE_SIZE      16
#define HORI_LINE_SIZE      22
#define NUM_HORI_CELLS       9
#define NUM_VERT_CELLS       7
#define NUM_HORI_LINES_COL   8
#define NUM_VERT_LINES_ROW  10

#define LEFT                 1
#define RIGHT                2
#define UP                   3
#define DOWN                 4
#define FALSE                0
#define TRUE                 1

#define VIDEOPAC_RES_W 200        // original console screen resolution width
#define VIDEOPAC_RES_H 160        // original console screen resolution height
#define JOYSTICK_DEAD_RANGE 8000  // dead-range - and + for analog joystick

#define NUM_SOUNDS 18

typedef struct horizontal_line_type {     // contatins 8 rows of 9 lines
  char line[NUM_HORI_CELLS + 1];
} horizontal_line_type;

horizontal_line_type horizontal_lines[NUM_HORI_LINES_COL];

typedef struct vertical_line_type {     // contatins 8 rows of 9 lines
  char line[NUM_VERT_LINES_ROW + 1];
} vertical_line_type;

vertical_line_type vertical_lines[NUM_VERT_CELLS];

typedef struct pill_type {
  int status,            // 0 = not active, 1 = normal, 2 = powerpill
      x,y,               // coordinates (top left)
      direction,         // 1=left, 2=right, 3=up, 4=down
      speed;             // in pixels per frame
} pill_type;

pill_type pills[99];    // max 99


typedef struct ghost_type {
  int colour,            // colour 1=yellow, 2=green, 3=red
                         //        5=magenta, 6=cyan, 7=white
      status,            // 1 = normal, 2 = magenta (can be eaten)
                         // 3 = eaten/dead, 4 = recharging in center
      recharge_timer,    // timer in frames for duration recharge
                         // only applicable for status 4
      x, y,              // coordinates x,y 
      direction,         // 1=left, 2=right, 3=up, 4=down
      speed;             // in pixels per frame
} ghost_type;

ghost_type ghosts[10];    // max 10


/* global variables */
int screen_width  = 1000;   // initial factor 5 van Videpac 5x200
int screen_height = 800;    // initial factor 5 van Videpac 5x160
int factor = 5;             // resize factor (relative to 200x160 screen resolution)
int full_screen;            // TRUE/FALSE

int use_joystick;
int num_joysticks;
int joy_left, joy_right, joy_up, joy_down;
int last_joystick_action;       // TRUE/FALSE   used in title screen only

int NUM_PILLS;                  // default 12 min 12 max 99
int NUM_GHOSTS;                 // default 4  min  1 max 10

int munchkin_x ;                // x-coordinate
int munchkin_y;                 // y-coordinate
int munchkin_auto_direction;    // if <> 0, then 1,2,3 or 4 for auto movement to cell
                                //          1=left, 2=right, 3=up, 4-down
int munchkin_last_direction;    // (0=stopped, 1 left, 2 right, 3 up, 4 down)
int munchkin_animation_frame;   // to display animations during movement
int munchkin_dying_animation;   // to display dying animations 
int munchkin_dying;             // TRUE/FALSE

int maze_center_open;           // 1=left, 2=right, 3=up, 4=down
int maze_completed;             // TRUE/FALSE  1=completed
int maze_completed_animations;  // counter for end of level animations
char maze_color;                // m for magenta and y for yellow
int MAZE_OFFSET_X;              // default  9   left top corner of maze x position
int MAZE_OFFSET_Y;              // default 23   left top corner of maze y position
int maze_selected;              // 1,2 3, or 4 : maze selected in title screen


int last_pill_speed_increased;  // has the speed of the last pill already increased? 
int powerpill_color;            // 1=magenta, 2=red, 3=cyan, 4=green
int powerpill_active_timer;     // timer for how long ghosts are magenta (can be eaten)


int speed;                      // munchkin speed in pixels per frame

int high_score_broken;          // TRUE/FALSE 0-not, 1 = true
int high_score_registration;    // TRUE/FALSE 0: not possible, 1: active
int high_score_character_pos;   // 0...6 active to enter
int score, high_score;
char high_score_name[7];        // max length is 6 charachters! 
int flash_high_score_timer;     // 0..150 frames reverse, timer for flashing high score name
                                //                        at end of every game

int frame;                      // frame counter
int start_delay;                // used to delay start screen

int vol_effects, vol_music;
Mix_Chunk * sounds[NUM_SOUNDS];

SDL_Joystick *js;


/* DATA_PREFIX              do not change the order/name/location of images
                            add new images at end                     */
const char * image_names[NUM_IMAGES] = {
  DATA_PREFIX "images/munchkin/line_horizontal_magenta_factor5.bmp",       // entry = 0  
  DATA_PREFIX "images/munchkin/line_vertical_magenta_factor5.bmp",         // entry = 1
  DATA_PREFIX "images/munchkin/munchkin_cyan_factor5.bmp",                 
  DATA_PREFIX "images/munchkin/munchkin_left_cyan_factor5.bmp", 
  DATA_PREFIX "images/munchkin/munchkin_right_cyan_factor5.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/munchkin_center_cyan_factor5.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp",                  // 7: shield, max 9x9 pixels
  DATA_PREFIX "images/munchkin/dummy.bmp",             // 8: shield, max 9x9 pixels
  DATA_PREFIX "images/munchkin/dummy.bmp",              // 9: shield, max 9x9 pixels
  DATA_PREFIX "images/munchkin/dummy.bmp",             // 10: shield or bullet, max 9x9 pixels

  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 

  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 

  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  DATA_PREFIX "images/munchkin/dummy.bmp", 
  
  DATA_PREFIX "images/munchkin/munchkin_up_cyan_factor5.bmp",       // 72
  DATA_PREFIX "images/munchkin/munchkin_down_cyan_factor5.bmp",     // 73
  DATA_PREFIX "images/munchkin/pill_white_factor5.bmp",              // 74
  DATA_PREFIX "images/munchkin/munchkin_cyan_closed_factor5.bmp",    // 75
  DATA_PREFIX "images/munchkin/munchkin_win_cyan_factor5.bmp",       // 76
  DATA_PREFIX "images/munchkin/line_horizontal_yellow_factor5.bmp",       // 77
  DATA_PREFIX "images/munchkin/line_vertical_yellow_factor5.bmp",         // 78
  DATA_PREFIX "images/munchkin/pill_magenta_factor5.bmp",             // 79  
  DATA_PREFIX "images/munchkin/pill_red_factor5.bmp",                 // 80    
  DATA_PREFIX "images/munchkin/pill_cyan_factor5.bmp",                // 81  
  DATA_PREFIX "images/munchkin/pill_green_factor5.bmp",               // 82
  DATA_PREFIX "images/munchkin/pill_flash_magenta_factor5.bmp",             // 83  
  DATA_PREFIX "images/munchkin/pill_flash_red_factor5.bmp",                 // 84    
  DATA_PREFIX "images/munchkin/pill_flash_cyan_factor5.bmp",                // 85  
  DATA_PREFIX "images/munchkin/pill_flash_green_factor5.bmp",               // 86

  DATA_PREFIX "images/munchkin/ghost_left1_yellow_factor5.bmp",             // 87
  DATA_PREFIX "images/munchkin/ghost_left2_yellow_factor5.bmp",             // 88
  DATA_PREFIX "images/munchkin/ghost_right1_yellow_factor5.bmp",             // 89
  DATA_PREFIX "images/munchkin/ghost_right2_yellow_factor5.bmp",             // 90
  DATA_PREFIX "images/munchkin/ghost_up1_yellow_factor5.bmp",                // 91
  DATA_PREFIX "images/munchkin/ghost_up2_yellow_factor5.bmp",                // 92
  DATA_PREFIX "images/munchkin/ghost_down1_yellow_factor5.bmp",              // 93
  DATA_PREFIX "images/munchkin/ghost_down2_yellow_factor5.bmp",               // 94

  DATA_PREFIX "images/munchkin/ghost_left1_green_factor5.bmp",             //   95
  DATA_PREFIX "images/munchkin/ghost_left2_green_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_right1_green_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_right2_green_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_up1_green_factor5.bmp",                //   
  DATA_PREFIX "images/munchkin/ghost_up2_green_factor5.bmp",                //  100 
  DATA_PREFIX "images/munchkin/ghost_down1_green_factor5.bmp",              //    
  DATA_PREFIX "images/munchkin/ghost_down2_green_factor5.bmp",               //  102 

  DATA_PREFIX "images/munchkin/ghost_left1_red_factor5.bmp",             // 103
  DATA_PREFIX "images/munchkin/ghost_left2_red_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_right1_red_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_right2_red_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_up1_red_factor5.bmp",                // 
  DATA_PREFIX "images/munchkin/ghost_up2_red_factor5.bmp",                // 
  DATA_PREFIX "images/munchkin/ghost_down1_red_factor5.bmp",              // 
  DATA_PREFIX "images/munchkin/ghost_down2_red_factor5.bmp",               // 110

  DATA_PREFIX "images/munchkin/ghost_left1_cyan_factor5.bmp",             // 111
  DATA_PREFIX "images/munchkin/ghost_left2_cyan_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_right1_cyan_factor5.bmp",            // 
  DATA_PREFIX "images/munchkin/ghost_right2_cyan_factor5.bmp",            // 
  DATA_PREFIX "images/munchkin/ghost_up1_cyan_factor5.bmp",               // 
  DATA_PREFIX "images/munchkin/ghost_up2_cyan_factor5.bmp",               // 
  DATA_PREFIX "images/munchkin/ghost_down1_cyan_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_down2_cyan_factor5.bmp",             // 118

  DATA_PREFIX "images/munchkin/ghost_left1_magenta_factor5.bmp",             // 119
  DATA_PREFIX "images/munchkin/ghost_left2_magenta_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_right1_magenta_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_right2_magenta_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_up1_magenta_factor5.bmp",                // 
  DATA_PREFIX "images/munchkin/ghost_up2_magenta_factor5.bmp",                // 
  DATA_PREFIX "images/munchkin/ghost_down1_magenta_factor5.bmp",              // 
  DATA_PREFIX "images/munchkin/ghost_down2_magenta_factor5.bmp",              // 126

  DATA_PREFIX "images/munchkin/ghost_left1_white_factor5.bmp",             // 127
  DATA_PREFIX "images/munchkin/ghost_left2_white_factor5.bmp",             //   
  DATA_PREFIX "images/munchkin/ghost_right1_white_factor5.bmp",            // 
  DATA_PREFIX "images/munchkin/ghost_right2_white_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_up1_white_factor5.bmp",               // 
  DATA_PREFIX "images/munchkin/ghost_up2_white_factor5.bmp",               // 
  DATA_PREFIX "images/munchkin/ghost_down1_white_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_down2_white_factor5.bmp",             // 134

  DATA_PREFIX "images/munchkin/ghost_left1_invisible_factor5.bmp",           // 135
  DATA_PREFIX "images/munchkin/ghost_left2_invisible_factor5.bmp",           //   
  DATA_PREFIX "images/munchkin/ghost_right1_invisible_factor5.bmp",          // 
  DATA_PREFIX "images/munchkin/ghost_right2_invisible_factor5.bmp",          // 
  DATA_PREFIX "images/munchkin/ghost_up1_invisible_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_up2_invisible_factor5.bmp",             // 
  DATA_PREFIX "images/munchkin/ghost_down1_invisible_factor5.bmp",           // 
  DATA_PREFIX "images/munchkin/ghost_down2_invisible_factor5.bmp",            // 142

  DATA_PREFIX "images/munchkin/munchkin_cyan_dying1_factor5.bmp",            // 143
  DATA_PREFIX "images/munchkin/munchkin_cyan_dying2_factor5.bmp",            // 144
  DATA_PREFIX "images/munchkin/munchkin_cyan_dying3_factor5.bmp",            // 145
  DATA_PREFIX "images/munchkin/munchkin_cyan_dying4_factor5.bmp",            // 146
  DATA_PREFIX "images/munchkin/munchkin_cyan_dying5_factor5.bmp"             // 147

};
  

const char * sound_names[NUM_SOUNDS] = {
  DATA_PREFIX "sounds/dummy.wav",        // 0
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/score.wav",
  DATA_PREFIX "sounds/character_beep.wav",  // 7 
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/dummy.wav",
  DATA_PREFIX "sounds/select_game.wav",                  // 10
  DATA_PREFIX "sounds/munchkin_walk_short_gain.wav",     // 11
  DATA_PREFIX "sounds/munchkin_eat_pill.wav",            // 12
  DATA_PREFIX "sounds/munchkin_maze_completed.wav",      // 13
  DATA_PREFIX "sounds/munchkin_eat_powerpill.wav",       // 14
  DATA_PREFIX "sounds/munchkin_ghost_eaten.wav",         // 15
  DATA_PREFIX "sounds/munchkin_ghosts_move.wav",         // 16
  DATA_PREFIX "sounds/munchkin_dying.wav"                // 17
};

SDL_Surface * screen;
SDL_Surface * images[NUM_IMAGES];

/* global font variables */
SDL_Surface * text;
TTF_Font * font_large;
TTF_Font * font_small;
int font_size;

#define _________________________a
#define ___FORWARD_DECLARATIONS__b
#define _________________________c

/* forward declarations of functions/procedures */
void title_screen();
void display_select_game(int x, int y);
void display_instructions(int scroll_x, int scroll_y);
void switch_active_mini_map(int maze_selected);
void display_active_option_row(int row);

int game(int mode);
void setup(void);
void setup_joystick();
void start_new_game();
void load_images();
int get_user_input();
void wait_for_no_left_right_event();
void cleanup();
void handle_screen_resize(int mode);

void setup_maze(int maze_nr);
void draw_maze();
void handle_maze_completed();

void draw_munchkin();

void start_new_maze(); 
void setup_pills();
void handle_pills();
void draw_pills();
int check_pill_eaten(int i);
void choose_pill_direction(int i);

void setup_ghosts();
void handle_ghosts();
void choose_ghost_direction (int i);
void draw_ghosts();
void check_ghosts_hits_munchkin();

void draw_score_line();
void flash_high_score_name();
void print_high_score_char(int character);
void play_sound(int snd, int chan);

#define __________a
#define ___MAIN___b
#define __________c

/* ------------ 
   -   MAIN   - 
   ------------ */

int main(int argc, char * argv[])
{
  int i, mode, quit;
  printf("Start\n");

  /* Stop any music: */
  Mix_HaltMusic();   

  full_screen = FALSE;
  for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-fullscreen") == 0 || strcmp(argv[i], "-f") == 0)
             full_screen = TRUE;
  }  

  setup();

  /* Call the cleanup function when the program exits */
  atexit(cleanup);

  srand( time(NULL) );

  /* Main loop */
  do {  
    title_screen();
    quit = game(mode);
  }  
  while (quit == 0);

  SDL_Quit();
  exit(0);
}



int game(int mode)
{
  int done, quit;
  Uint32 last_time;
   
  frame = 0;
  done = 0;
  quit = 0;
  high_score_broken = FALSE;
  high_score_registration = FALSE;

  // wait until key/joystick left or right is released to prevent 
  // immediate move munchkin from start
  wait_for_no_left_right_event();
      
  start_new_game();



  /* ------------------
     - Main game loop -
     ------------------ */
  do
  {
      last_time = SDL_GetTicks();
      frame++;

      SDL_Flip(screen);

      /* restart_game after death */
      if (munchkin_dying == TRUE && munchkin_dying_animation == 25) {
        start_new_game();
      }  

      /* restart maze after completion */
      if (maze_completed == TRUE && maze_completed_animations == 0) {
        start_new_maze();
      }  

      done = get_user_input();   
      SDL_FillRect(screen, NULL, 0);  /* Blank the screen */
    
      draw_munchkin();
      if (maze_completed == TRUE) handle_maze_completed();
      handle_pills();
      draw_pills();
      if (munchkin_dying == FALSE || munchkin_dying_animation == 0) handle_ghosts();
      if (munchkin_dying == FALSE || munchkin_dying_animation <= 3) draw_ghosts();
      if (maze_completed == FALSE) check_ghosts_hits_munchkin();
      draw_maze('y');       // in yellow
      draw_score_line(); 

      /* Pause till next frame: */
      if (SDL_GetTicks() < last_time + 33)
          SDL_Delay(last_time + 33 - SDL_GetTicks());
    }
  while (!done && !quit);
  
  return(0);
}



void setup(void)
{
  int i;
  char title_string[100];

  /* Init SDL Video: */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      fprintf(stderr,
              "\nError: I could not initialize video!\n"
              "The Simple DirectMedia error that occured was:\n"
              "%s\n\n", SDL_GetError());
      exit(1);
    }

    /* enable console output in Windows */
    #ifdef _WIN32
    freopen("CON", "w", stdout); // redirects stdout
    freopen("CON", "w", stderr); // redirects stderr
    #endif

   /* Init True Type Font */
   if (TTF_Init() < 0) 
   { 
      fprintf(stderr, "Impossible to initialize SDL_TTF: %s\n",SDL_GetError() );
      exit(1);
   }
   font_size = 60;
   font_large = TTF_OpenFont("O2.ttf", font_size);
   if (!font_large)
      fprintf(stderr, "Cannot load font name O2.ttf large: %s\n", SDL_GetError());

   font_small = TTF_OpenFont("O2.ttf", font_size/2);
   if (!font_small)
      fprintf(stderr, "Cannot load font name O2.ttf small: %s\n", SDL_GetError());

   high_score = 0; 
   strcpy(high_score_name, "??????");

  /* Open display: */
  if (full_screen == TRUE) {    // command line option set
        screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT | SDL_FULLSCREEN);
        if (screen == NULL) {
              fprintf(stderr, "\nWarning: I could not set up video for "
                      "full-screen.\n"
                      "The Simple DirectMedia error that occured was:\n"
                      "%s\n\n", SDL_GetError());
         }
   } else {      
      screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
      if (screen == NULL)
        {
          fprintf(stderr,
                  "\nWarning: I could not set up video for "
                  "1000x800 mode.\n"
                  "The Simple DirectMedia error that occured was:\n"
                  "%s\n\n", SDL_GetError());
        } 
    }  // if full screen    

  setup_joystick();

  /* Set window manager stuff: */
  sprintf(title_string, "MUNCHKIN - factor: %d ", factor);
  SDL_WM_SetCaption(title_string, "MUNCHKIN");

  load_images();

  /* Open sound */

  if (Mix_OpenAudio(22050, AUDIO_S16, 1, 1024) < 0) {
         fprintf(stderr,
          "\nWarning: I could not set up audio for 22050 Hz "
          "16-bit stereo.\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", SDL_GetError());
          exit(1);
  }
        
  vol_effects = 5;
  vol_music = 5;
  
  Mix_Volume(-1, vol_effects * (MIX_MAX_VOLUME / 5));
  Mix_VolumeMusic(vol_music * (MIX_MAX_VOLUME / 5));

  Mix_AllocateChannels(32);

 /* Load sounds */
      
 for (i = 0; i < NUM_SOUNDS; i++) {
    sounds[i] = Mix_LoadWAV(sound_names[i]);
    if (sounds[i] == NULL)
      {
        fprintf(stderr,
          "\nError: I could not load the sound file:\n"
          "%s\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", sound_names[i], SDL_GetError());
        exit(1);
      }
  }  

  NUM_PILLS = 12;                  // default 12 min 12 max 99
  NUM_GHOSTS = 4;                  // default 4  min  1 max 10
  MAZE_OFFSET_X =  9;   // left top corner of maze x position (factor 1)
  MAZE_OFFSET_Y = 23;   // left top corner of maze y position
  setup_pills();
  maze_selected = 1;

}   


void setup_maze(int maze_nr)
{
  char text_hori_line[NUM_HORI_CELLS + 1];        // + 1 for end of string
  char text_vert_line[NUM_VERT_LINES_ROW + 1];    // + 1 for end of string
  
  switch (maze_nr) {
  case 1:
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[0].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|---|----|"); sprintf(vertical_lines[0].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x---x-x-");  sprintf(horizontal_lines[1].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|--|-----|"); sprintf(vertical_lines[1].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "----xx---");  sprintf(horizontal_lines[2].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|||--|-|||"); sprintf(vertical_lines[2].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "--x---x--");  sprintf(horizontal_lines[3].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|----|-|"); sprintf(vertical_lines[3].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x---x---x");  sprintf(horizontal_lines[4].line, text_hori_line);
  sprintf(text_vert_line, "%s", "---||||---"); sprintf(vertical_lines[4].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x-------x");  sprintf(horizontal_lines[5].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|----|-|"); sprintf(vertical_lines[5].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x--x-xx-");  sprintf(horizontal_lines[6].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|--|-|---|"); sprintf(vertical_lines[6].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[7].line, text_hori_line);
  break;

  case 2:
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[0].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|-|-|--|"); sprintf(vertical_lines[0].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x---x-x-");  sprintf(horizontal_lines[1].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|--|-|---|"); sprintf(vertical_lines[1].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x-x--x-x");  sprintf(horizontal_lines[2].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|---|--|"); sprintf(vertical_lines[2].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x--x-x-x-");  sprintf(horizontal_lines[3].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|--|---|-|"); sprintf(vertical_lines[3].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x-x-x-x-x");  sprintf(horizontal_lines[4].line, text_hori_line);
  sprintf(text_vert_line, "%s", "----||----"); sprintf(vertical_lines[4].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x-x---x-x");  sprintf(horizontal_lines[5].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||-|--|--|"); sprintf(vertical_lines[5].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "---xxx-x-");  sprintf(horizontal_lines[6].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|----|-|"); sprintf(vertical_lines[6].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[7].line, text_hori_line);
  break;

  case 3:
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[0].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|-|--|-|"); sprintf(vertical_lines[0].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "--x--x---");  sprintf(horizontal_lines[1].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||--|-|-||"); sprintf(vertical_lines[1].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-------x-");  sprintf(horizontal_lines[2].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-||-||-||"); sprintf(vertical_lines[2].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "----x----");  sprintf(horizontal_lines[3].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||-|---|-|"); sprintf(vertical_lines[3].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x---x-x-x");  sprintf(horizontal_lines[4].line, text_hori_line);
  sprintf(text_vert_line, "%s", "--|-||-|--"); sprintf(vertical_lines[4].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x-------x");  sprintf(horizontal_lines[5].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|-|||-|--|"); sprintf(vertical_lines[5].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-----x-x-");  sprintf(horizontal_lines[6].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||-|--|--|"); sprintf(vertical_lines[6].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[7].line, text_hori_line);
  break;
  
  case 4: 
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[0].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||---|---|"); sprintf(vertical_lines[0].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-xxx--x-x");  sprintf(horizontal_lines[1].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|----|-|-|"); sprintf(vertical_lines[1].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "--x-x----");  sprintf(horizontal_lines[2].line, text_hori_line);
  sprintf(text_vert_line, "%s", "||-|-||-||"); sprintf(vertical_lines[2].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x---x---");  sprintf(horizontal_lines[3].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|---||-|-|"); sprintf(vertical_lines[3].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "x-xxx-xxx");  sprintf(horizontal_lines[4].line, text_hori_line);
  sprintf(text_vert_line, "%s", "----|||---"); sprintf(vertical_lines[4].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "xx---x--x");  sprintf(horizontal_lines[5].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|--||--|-|"); sprintf(vertical_lines[5].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "-x---x-x-");  sprintf(horizontal_lines[6].line, text_hori_line);
  sprintf(text_vert_line, "%s", "|---|----|"); sprintf(vertical_lines[6].line,   text_vert_line);
  sprintf(text_hori_line, "%s", "xxxxxxxxx");  sprintf(horizontal_lines[7].line, text_hori_line);
  break;
  }   // end switch
} 


void setup_joystick()
{
  use_joystick = 1;
  num_joysticks = 0;
  joy_up = 0; joy_down = 0; joy_left = 0; joy_right = 0;
  last_joystick_action = FALSE;

  if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
      fprintf(stderr,
        "\nWarning: I could not initialize joystick.\n"
        "The Simple DirectMedia error that occured was:\n"
        "%s\n\n", SDL_GetError());
      
      use_joystick = 0;
  }
  else
    {
      /* Look for joysticks */
      
      num_joysticks = SDL_NumJoysticks();
      if (num_joysticks <= 0) {
         fprintf(stderr, "\nWarning: No joysticks available.\n");
      use_joystick = 0;
      }
      else
  {
    /* Open joystick */
    
    js = SDL_JoystickOpen(0);
    if (js == NULL) {
        fprintf(stderr,
          "\nWarning: Could not open joystick 1.\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", SDL_GetError());
         use_joystick = 0;
      }
    else
      {
        /* Check for proper stick configuration: */
        
        if (SDL_JoystickNumAxes(js) < 2) {
      fprintf(stderr, "\nWarning: Joystick doesn't have enough axes!\n");
      use_joystick = 0;
    }
        else
    {
      if (SDL_JoystickNumButtons(js) < 2) {
          fprintf(stderr,
            "\nWarning: Joystick doesn't have enough "
            "buttons!\n");
          use_joystick = 0;
        }
    }
    }
  }
  }

}


void start_new_game() 
{
  int i;

  munchkin_dying = FALSE;
  score = 0;
  high_score_broken = FALSE;

  start_delay = frame;
  start_new_maze();
}


void start_new_maze() 
{
  int i;

  munchkin_x = (89 + 7) * factor; // 9 + 4*20 = MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2))
  munchkin_y = (65 + 4) * factor; //23 + 3*14 = MAZE_OFFSET_Y + 3 * (VERT_LINE_SIZE -2)
  munchkin_auto_direction  = 0;    // stationary 
  munchkin_last_direction  = 0;    // stationary 
  munchkin_animation_frame = 0;
  munchkin_dying_animation = 0;
  maze_center_open = DOWN;            // 4=down open at startup
  maze_completed = FALSE;              // 0,1 , 1=completed
  maze_color = 'm';
  last_pill_speed_increased = 0;
  powerpill_color = 1;
  powerpill_active_timer = 0;

  speed = 5;           // default 5, super fast : 10 (no other values possible)
  joy_left = 0;
  joy_right = 0;
  joy_up = 0;
  joy_down = 0;
  
  setup_maze(maze_selected);     // to make sure center down is open
  setup_pills();
  setup_ghosts();
}



void load_images(void)
{
  int i;
  char image_string[200];
  char temp_string[200];
  SDL_Surface * image;


  for (i = 0; i <  NUM_IMAGES; i++)  
  {
    strcpy(image_string, image_names[i] );
    image = SDL_LoadBMP(image_string);

    if (image == NULL)
    {
      fprintf(stderr,
        "\nError: I couldn't load a graphics file:\n"
        "%s\n"
        "The Simple DirectMedia error that occured was:\n"
        "%s\n\n", image_string, SDL_GetError());
      exit(1);
    }
      
    /* Convert to display format: */
    images[i] = SDL_DisplayFormat(image);
    if (images[i] == NULL)
    {
        fprintf(stderr,
                "\nError: I couldn't convert a file to the display format:\n"
                "%s\n"
                "The Simple DirectMedia error that occured was:\n"
                "%s\n\n", image_names[i], SDL_GetError());
        exit(1);
    }

    /* Set transparency: */
    if (i != 10)       // do not set transparency for white gun bit and bullets
    {  
       if (SDL_SetColorKey(images[i], (SDL_SRCCOLORKEY | SDL_RLEACCEL),
               SDL_MapRGB(images[i] -> format,
               0xFF, 0xFF, 0xFF)) == -1)
        {
           fprintf(stderr,
             "\nError: I could not set the color key for the file:\n"
             "%s\n"
             "The Simple DirectMedia error that occured was:\n"
             "%s\n\n", image_names[i], SDL_GetError());
           exit(1);
        }
    }

  }  // end for loop
}  


int get_user_input()
{
    SDL_Event event;
    Uint8* keystate = SDL_GetKeyState(NULL);
    int rotate_gun = 0;
    int window_size_changed;
    char title_string[100];
    int i;
    int munchkin_direction;   //1=left, 2=right, 3=up, 4=down
    int munchkin_manual_move; //0=no  1=yes
    int cell_x, cell_y;

    window_size_changed = 0;

     if ( keystate[56] && munchkin_dying != 1) { // toggle full_screen: 8 key
        window_size_changed = 1;
        screen_width = 1000;
        screen_height = 800;
        if (full_screen == TRUE) {
           full_screen = FALSE;
        } else { 
           full_screen = TRUE;
        }
        factor = 5;  // always to factor 5
     }

     if (window_size_changed == 1) {
        if (full_screen == TRUE) {
          screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT | SDL_FULLSCREEN);
          if (screen == NULL)
          {
            fprintf(stderr, "\nWarning: I could not set up video for "
                    "full-screen.\n"
                    "The Simple DirectMedia error that occured was:\n"
                    "%s\n\n", SDL_GetError());
          }
        } else { 
            handle_screen_resize(2);
        }
      };


  /* Loop through waiting messages and process them */
  
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      /* Closing the Window will exit the program */
      case SDL_QUIT:   // close window
        exit(0);
      break;

      case SDL_KEYDOWN:

        if (event.key.keysym.sym == SDLK_ESCAPE ) {
            printf("--Escape\n");   // return to instructions
            start_new_game();           // clear all objects
            return(1);
        } else {
          if ( (event.key.keysym.sym >= 97 && event.key.keysym.sym <= 122)
                 || event.key.keysym.sym == 32 || event.key.keysym.sym == 13) {    // spatie, return
            if (high_score_registration == TRUE) {
              print_high_score_char(event.key.keysym.sym);
            } 
          }
        }
      break;

      case SDL_JOYBUTTONDOWN:
          if (  (event.jbutton.button == 0 || event.jbutton.button == 1)
                && munchkin_dying != 1 )
               ; //printf("Fire button pressed\n");
          if (event.type == SDL_JOYBUTTONDOWN  && event.jbutton.button == 8) {  // home button joy
                 printf("--Escape pressed joystick\n");   // return to instructions
                 start_new_game();           // clear all objects
                 return(1);
          }
          
      break;

      case SDL_JOYAXISMOTION:
        //If joystick 0 has moved
        if ( event.jaxis.which == 0 ) {
            //If the X axis changed
            if ( event.jaxis.axis == 0 ) {
                //If the X axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_left = 0;
                     joy_right = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                        joy_left = 1;
                        joy_right = 0;
                    } else {
                        joy_left = 0;
                        joy_right = 1;
                    }
                }
            }
            //If the Y axis changed
            else if ( event.jaxis.axis == 1 ) {
                //If the Y axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_up = 0;
                     joy_down = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                     joy_up = 1;
                     joy_down = 0;
                    } else {
                        joy_up = 0;
                        joy_down = 1;
                    }
                }
            }
        }   // if event.jaxis.which == 0      
      break;      

    }  // end switch
  }    // end while


   munchkin_direction = 0;
   munchkin_manual_move = 0;

   /* Check continuous-response keys  */

   if (munchkin_dying == FALSE) {

       if (keystate[SDLK_LEFT] || joy_left == 1) {
           if (munchkin_auto_direction == UP || munchkin_auto_direction == DOWN) {   
               ;  // // complete current auto move
           } else {
                    if (munchkin_manual_move == 0) {
                        munchkin_direction = LEFT;
                        munchkin_manual_move = 1;
                     }    
           }
       }
       if (keystate[SDLK_RIGHT] || joy_right == 1) { 
           if (munchkin_auto_direction == UP || munchkin_auto_direction == DOWN) {   
               ;  // // complete current auto move
           } else {
                    if (munchkin_manual_move == 0) {
                        munchkin_direction = RIGHT;
                        munchkin_manual_move = 1;
                     }   
           }    
       }
       if (keystate[SDLK_UP] || joy_up == 1) { 
           if (munchkin_auto_direction == LEFT || munchkin_auto_direction == RIGHT) {   
               ;  // // complete current auto move
           } else {
                   if (munchkin_manual_move == 0) {
                       munchkin_direction = UP;
                       munchkin_manual_move = 1;
                   }    
           }    
       }    
       if (keystate[SDLK_DOWN] || joy_down == 1) { 
           if (munchkin_auto_direction == LEFT || munchkin_auto_direction == RIGHT) {   
               ;  // // complete current auto move
           } else {
                   if (munchkin_manual_move == 0) {
                       munchkin_direction = DOWN;
                       munchkin_manual_move = 1;
                   }    
           }    
       }

       munchkin_last_direction = munchkin_direction;


       // when no key pressed and munchkin_auto_direction <> 0
       // move automatically in last direction

       // calculate cel number (starting point 0,0)
       cell_x = ( (munchkin_x / factor) - (7 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
       cell_y = ( (munchkin_y / factor) - (4 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  

       //printf("cell_x %d cell_y %d line %c dy %d\n", cell_x, cell_y,horizontal_lines[cell_y].line[cell_x],
       //     ((MAZE_OFFSET_Y + 4 + ((cell_y) * (VERT_LINE_SIZE - 2))) * factor) );


       if (maze_completed == FALSE) {   

           if (munchkin_manual_move == 1) {  // left
               switch (munchkin_direction) {

                 case LEFT: 
                   if (vertical_lines[cell_y].line[cell_x] == '|' && 
                       ((munchkin_x - speed) < ((MAZE_OFFSET_X + 7 + (cell_x) * (HORI_LINE_SIZE - 2))) * factor)
                                                     // 7 munchkin offset in cel
                       ) {
                            ; // continue left (not at center of cell yet)
                    } else { 
                       if ( (munchkin_last_direction == UP || munchkin_last_direction == DOWN) 
                            && 
                            ( (munchkin_y - 135) % 70 == 0)   ) {  // change direction only if on boundery
                            munchkin_x = munchkin_x - speed;
                            munchkin_auto_direction = LEFT;
                            munchkin_last_direction = LEFT;
                        } else { 
                                 if (munchkin_last_direction == LEFT || munchkin_last_direction == RIGHT
                                     || munchkin_last_direction == 0  ) { //  than movement allowed
                                       munchkin_x = munchkin_x - speed;
                                       munchkin_auto_direction = LEFT;
                                       munchkin_last_direction = LEFT;
                                   }
                               }   
                    }
                    if (munchkin_x < -20) munchkin_x = 980;  // wrap screen left
                 break;

                 case RIGHT: 
                   if (vertical_lines[cell_y].line[cell_x + 1] == '|' && 
                       (munchkin_x + speed) > ((MAZE_OFFSET_X + 7 + ((cell_x) * (HORI_LINE_SIZE - 2))) * factor)
                                                  // 7 munchkin offset in cel
                       ) { ;  // continue right (not at center of cell yet)
                    } else {
                       if ( (munchkin_last_direction == UP || munchkin_last_direction == DOWN) 
                            && 
                            ( (munchkin_y - 135) % 70 == 0)   ) {  // change direction only if on boundery
                            munchkin_x = munchkin_x + speed;
                            munchkin_auto_direction = RIGHT;
                            munchkin_last_direction = RIGHT;
                        } else { ;
                                 if (munchkin_last_direction == LEFT || munchkin_last_direction == RIGHT
                                     || munchkin_last_direction == 0  ) { //  than movement allowed
                                       munchkin_x = munchkin_x + speed;
                                       munchkin_auto_direction = RIGHT;
                                       munchkin_last_direction = RIGHT;
                                   }
                               }   
                    }
                    if (munchkin_x > 980) munchkin_x = -20;  // wrap screen right
                 break;

                 case UP:  
                   if (horizontal_lines[cell_y].line[cell_x] == 'x' && 
                       (munchkin_y - speed) < ((MAZE_OFFSET_Y + 4 + ((cell_y) * (VERT_LINE_SIZE - 2))) * factor)
                                                  // 4 munchkin offset in cel
                       ) {
                          ;  // continue up (not at center of cell yet)
                    } else { 
                       if ( (cell_x == -1 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) {
                          ;   // if munchkin outside maze (wrap via tunnel) do not allow UP
                       } else {
                          munchkin_y = munchkin_y - speed;
                          munchkin_auto_direction = UP;
                          munchkin_last_direction = UP;
                       }   

                    }
                 break;

                 case DOWN: 
                   if (horizontal_lines[cell_y + 1].line[cell_x] == 'x' && 
                       (munchkin_y + speed) > ((MAZE_OFFSET_Y + 4 + ((cell_y) * (VERT_LINE_SIZE - 2))) * factor)
                                                  // 4 munchkin offset in cel
                       ) {
                            ;  // continue down (not at center of cell yet)
                    } else {
                       
                       if ( (cell_x == -1 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) {
                          ;   // if munchkin outside maze (wrap via tunnel) do not allow DOWN
                       } else {
                          munchkin_y = munchkin_y + speed;
                          munchkin_auto_direction = DOWN;
                          munchkin_last_direction = DOWN;
                       } 
                    }                  
                 break;
               }   // switch
               }   // munchkin_manual_move == 1
      
           // auto direction
           // if munchkin at boundery of cell, stop auto movement
           //    only if automovement move munchkin
           if (munchkin_auto_direction +! 0 && munchkin_manual_move == 0) {    // if no key pressed but auto move
             switch (munchkin_auto_direction) {
                 case LEFT: 
                   if ( (munchkin_x - 80) % 100 == 0) munchkin_auto_direction = 0;
                   else { munchkin_x = munchkin_x - speed;
                          if (munchkin_x < -20) munchkin_x = 980;    // wrap screen left
                          munchkin_last_direction = LEFT;
                        }  
                 break;  
                 case RIGHT: 
                   if ( (munchkin_x - 80) % 100 == 0) munchkin_auto_direction = 0;
                   else { munchkin_x = munchkin_x + speed;
                          if (munchkin_x > 980) munchkin_x = -20;    // wrap screen right
                          munchkin_last_direction = RIGHT;
                        }  
                 break;  
                 case UP:
                   if ( (munchkin_y - 135) % 70 == 0) munchkin_auto_direction = 0;
                   else { munchkin_y = munchkin_y - speed;
                          munchkin_last_direction = UP;
                        }  
                 break;  
                 case DOWN: 
                   if ( (munchkin_y - 135) % 70 == 0) munchkin_auto_direction = 0;
                   else { munchkin_y = munchkin_y + speed;
                          munchkin_last_direction = DOWN;
                        }  
                 break;  
             }
           }   // if no key pressed but auto move

             if (munchkin_manual_move != 0 || munchkin_auto_direction != 0 ) play_sound(11, 1); 
                    else play_sound(16, 6);  
             
      }  // if maze_completed == FALSE   
    } // dying

   /* handle fire key */
   if ( (keystate[SDLK_LCTRL] || keystate[SDLK_RCTRL])  && munchkin_dying != 1 ) {
       ; // prevent bullets fired to soon after each other
    }
 return(0);
}


void cleanup()
{
  /* Shut down SDL */
  printf("Exit game, cleaning up\n");
  Mix_HaltMusic();
  Mix_HaltChannel(-1);
  if (use_joystick == 1) SDL_JoystickClose(js);
  TTF_CloseFont(font_large);
  TTF_CloseFont(font_small);
  SDL_FreeSurface(text);
  SDL_FreeSurface(screen);
  SDL_Quit();
}


void handle_screen_resize(int mode)
{
  // mode == 1: in instructions screen, mode == 2: during game play, not used anymore
  int i;
  char title_string[100];

  screen_width  =  factor * VIDEOPAC_RES_W;           
  screen_height =  factor * VIDEOPAC_RES_H;
  screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
  if (screen == NULL)
    {
      fprintf(stderr, "\nWarning: I could not set up video for "
              "larger/smaller mode.\n"
              "The Simple DirectMedia error that occured was:\n"
              "%s\n\n", SDL_GetError());
    }

  sprintf(title_string, "MUNCHKIN - factor: %d maze: : %d", factor, maze_selected);
  SDL_WM_SetCaption(title_string, "MUNCHKIN");

  SDL_Flip(screen);
  SDL_Delay(500);
}


void draw_munchkin()
{
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)
  int image_num;

  src_rect.x = 0;  // left
  src_rect.y = 0;  // up
  src_rect.w = 8 * factor;   
  src_rect.h = 8 * factor;   

  rect.x = munchkin_x;   // x position  on screen
  rect.y = munchkin_y;   // y postition on scherm
  rect.w = 8 * factor;    // ignored!
  rect.h = 8 * factor;    // ignored!

  if (maze_completed == FALSE) {
      // determine which image to display
      switch (munchkin_last_direction) {
      case 0:
          image_num = 2;     // stationary
          break;
      case LEFT:
          image_num = 3;     // left
          break;
      case RIGHT:
          image_num = 4;     // right
          break;
      case UP:
          image_num = 72;     // up
          break;
      case DOWN:
          image_num = 73;     // down
          break;
      }

        if (munchkin_last_direction != 0) {    // only if moving 
            if (munchkin_animation_frame == 0 || munchkin_animation_frame == 1 || munchkin_animation_frame == 2)
                 image_num = 6;    // close image,  animation toggle 3 frames delay
            munchkin_animation_frame ++;
            if (munchkin_animation_frame == 6)  munchkin_animation_frame = 0;
      } 

      }  else {    // maze completed, animate munchkin   
            //printf("maze color %c\n", maze_color);
            if (maze_color == 'm')
              image_num = 76;  // munchkin mouth open
            else 
              image_num = 75;  // munchkin mouth close
  }   // if maze_completed == FALSE
  

  if (munchkin_dying == TRUE) {
      // determine which image to display
      switch (munchkin_dying_animation) {
      case 1:
          image_num = 76;     
          break;
      case 2:
          image_num = 75;     
          break;
      case 3:
          image_num = 143;     
          break;
      case 4:
          image_num = 144;    
          break;
      case 5:
          image_num = 145;    
          break;
      case 6:
          image_num = 146;    
          break;
      case 7:
          image_num = 147;    
          //printf("GAME OVER\n");
          flash_high_score_timer = 55;  // +/- 5 seconds : flashing highscore name
          break;
      }
      //printf("Dying image_num: %d dying_animation_frame %d\n", image_num, munchkin_dying_animation);
      if (frame % 7 == 0) {        // increase animation every x frames
        munchkin_dying_animation++;
        if (munchkin_dying_animation >= 8) {   // ?kan weg
             if (munchkin_dying_animation == 25) {   // delay before name can be entered
                if (high_score_broken == TRUE) {
                      high_score_registration = TRUE;   
                      high_score_character_pos = 0;
                      //printf("Name can be entered\n");
                 }     
             }  
        }
       }
      
   }  // munchkin_dying


  if (munchkin_dying == FALSE || munchkin_dying_animation < 8)
    SDL_BlitSurface(images[image_num], &src_rect, screen, &rect);

}


void draw_maze()       //maze color: y=yellow, m=magenta
{
  int i,j;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)
  char text_hori_line[NUM_HORI_CELLS + 1];        // + 1 for end of string
  char text_vert_line[NUM_VERT_LINES_ROW + 1];    // + 1 for end of string

  // change maze center opening (skip first time)
  
  if (frame % 45 == 0) {      // rotate every 45 frames clockwise
        if (maze_center_open == DOWN) maze_center_open = LEFT;
        else if (maze_center_open == LEFT) maze_center_open = UP;
          else if (maze_center_open == UP) maze_center_open = RIGHT;
             else if (maze_center_open == RIGHT) maze_center_open = DOWN;
  }
    

  switch (maze_center_open) {
  case LEFT:    
    horizontal_lines[4].line[4] = 'x';
    vertical_lines[4].line[4] = '-'; vertical_lines[4].line[5] = '|';
    horizontal_lines[5].line[4] = 'x';
    break;
  case RIGHT:    
    horizontal_lines[4].line[4] = 'x';
    vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '-';
    horizontal_lines[5].line[4] = 'x';
    break;
  case UP:    
    horizontal_lines[4].line[4] = '-';
    vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '|';
    horizontal_lines[5].line[4] = 'x';

    break;
  case DOWN:    
    horizontal_lines[4].line[4] = 'x';
    vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '|';
    horizontal_lines[5].line[4] = '-';
    break;
  }


  // Draw horizontal lines maze
  if (1 != 1)  {  //(munchkin_auto_direction >= 1 || munchkin_last_direction != 0) { // invisible
    ;
  } else {
      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = HORI_LINE_SIZE * factor;   
      src_rect.h = 2 * factor;   
      
      for (j = 0; j < NUM_HORI_LINES_COL; j++)
      {  
        for (i = 0; i < NUM_HORI_CELLS ; i++)  
        {
          //printf("j=%d, i=%d , tekens: %c\n", j, i, horizontal_lines[j].line[i]);
          if (horizontal_lines[j].line[i] == 'x') {
            rect.x = (MAZE_OFFSET_X + i*(HORI_LINE_SIZE-2)) * factor;
            rect.y = (MAZE_OFFSET_Y + j*(VERT_LINE_SIZE-2)) * factor;
            if (maze_color == 'm') SDL_BlitSurface(images[0], &src_rect, screen, &rect);
            else                   SDL_BlitSurface(images[77], &src_rect, screen, &rect);
          }
        }
      }

      // Draw vertical lines maze

      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = 2 * factor;   
      src_rect.h = VERT_LINE_SIZE * factor;  

      for (j = 0; j < NUM_VERT_CELLS; j++) 
      {
        for (i = 0; i < NUM_VERT_LINES_ROW ; i++)  
        {
          if (vertical_lines[j].line[i] == '|') {
            rect.x = (MAZE_OFFSET_X + i*(HORI_LINE_SIZE-2)) * factor;                           
            rect.y = (MAZE_OFFSET_Y + j*(VERT_LINE_SIZE-2)) * factor;    
            if (maze_color == 'm') SDL_BlitSurface(images[1], &src_rect, screen, &rect);
            else                   SDL_BlitSurface(images[78], &src_rect, screen, &rect);
          }  
        }
      }
  }  // if munchkin movement

}


void handle_maze_completed()
{
  ;
  ; // change maze color to yellow and magenta
  ; // smiling munchkin
  maze_completed_animations --;

  if (maze_completed_animations % 10 == 0) {
    if (maze_color == 'm' ) maze_color = 'y';
    else maze_color = 'm';
  }
}



void setup_pills()
{
  int i;

  /* first pil
  pill_x = (89 + 9) *  factor; // 9 + 4*20 = MAZE_OFFSET_X + 9
           //(9 + 9 + 0*20) * factor;
  pill_y = (65 + 9) * factor; //23 + 3*14 = MAZE_OFFSET_Y + 7
           //(23 + 7 + 0*14) * factor; */
 
  // top-left
  if (NUM_PILLS >= 1) {
     pills[0].x = (MAZE_OFFSET_X  + 9 + 0*20) * factor;
     pills[0].y = (MAZE_OFFSET_Y + 7 + 0*14) * factor;
     pills[0].direction = 4;                
     pills[0].status = 2;        // powerpill
  }   
  if (NUM_PILLS >= 2) {
     pills[1].x = (MAZE_OFFSET_X  + 9 + 1*20) * factor;
     pills[1].y = (MAZE_OFFSET_Y + 7 + 0*14) * factor;
     pills[1].direction = 1;                
     pills[1].status = 1;     
  }   
  if (NUM_PILLS >= 3) {
     pills[2].x = (MAZE_OFFSET_X  + 9 + 0*20) * factor;
     pills[2].y = (MAZE_OFFSET_Y + 7 + 1*14) * factor;
     pills[2].direction = 2;                
     pills[2].status = 1;     
  }   
  // top-right
  if (NUM_PILLS >= 4) {
     pills[3].x = (MAZE_OFFSET_X  + 9 + 7*20) * factor;
     pills[3].y = (MAZE_OFFSET_Y + 7 + 0*14) * factor;
     pills[3].direction = 1;                
     pills[3].status = 1;     
  }   
  if (NUM_PILLS >= 5) {
     pills[4].x = (MAZE_OFFSET_X  + 9 + 8*20) * factor;
     pills[4].y = (MAZE_OFFSET_Y + 7 + 0*14) * factor;
     pills[4].direction = 4;                
     pills[4].status = 2;        // powerpill
  }   
  if (NUM_PILLS >= 6) {
     pills[5].x = (MAZE_OFFSET_X  + 9 + 8*20) * factor;
     pills[5].y = (MAZE_OFFSET_Y + 7 + 1*14) * factor;
     pills[5].direction = 1;                
     pills[5].status = 1;     
  }   
  // bottom-left
  if (NUM_PILLS >= 7) {
     pills[6].x = (MAZE_OFFSET_X  + 9 + 0*20) * factor;
     pills[6].y = (MAZE_OFFSET_Y + 7 + 5*14) * factor;
     pills[6].direction = 2;                
     pills[6].status = 1;     
  }   
  if (NUM_PILLS >= 8) {
     pills[7].x = (MAZE_OFFSET_X  + 9 + 0*20) * factor;
     pills[7].y = (MAZE_OFFSET_Y + 7 + 6*14) * factor;
     pills[7].direction = 3;                
     pills[7].status = 2;       // powerpill
  }   
  if (NUM_PILLS >= 9) {
     pills[8].x = (MAZE_OFFSET_X  + 9 + 1*20) * factor;
     pills[8].y = (MAZE_OFFSET_Y + 7 + 6*14) * factor;
     pills[8].direction = 2;                
     pills[8].status = 1;     
  }   
  // bottom-right
  if (NUM_PILLS >= 10) {
     pills[9].x = (MAZE_OFFSET_X  + 9 + 8*20) * factor;
     pills[9].y = (MAZE_OFFSET_Y + 7 + 5*14) * factor;
     pills[9].direction = 1;                
     pills[9].status = 1;     
  }   
  if (NUM_PILLS >= 11) {
     pills[10].x = (MAZE_OFFSET_X  + 9 + 7*20) * factor;
     pills[10].y = (MAZE_OFFSET_Y + 7 + 6*14) * factor;
     pills[10].direction = 1;                
     pills[10].status = 1;     
  }   
  if (NUM_PILLS >= 12) {
     pills[11].x = (MAZE_OFFSET_X  + 9 + 8*20) * factor;
     pills[11].y = (MAZE_OFFSET_Y + 7 + 6*14) * factor;
     pills[11].direction = 3;                
     pills[11].status = 2;       // powerpill
  }   

  if (NUM_PILLS >= 13) {  // spread the rest of the pills random across to entire maze 
    for (i = 12; i < NUM_PILLS; i++) {
      pills[i].x = (MAZE_OFFSET_X  + 9 + ( rand()%8 ) *20) * factor;  //random cell x between 0 and 8
      pills[i].y = (MAZE_OFFSET_Y + 7 + ( rand()%6 ) *14) * factor;  //random cell x between 0 and 8
      pills[i].status = 1; 
      pills[i].direction = 2;    // must have value for choose_pill_direction
      choose_pill_direction(i);
      //printf("extra pill %d created, cell x %d, cell y %d, direction %d\n", i, 
      //         ((pills[i].x / factor) - (9 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2) ,  
      //         ((pills[i].y / factor) - (7 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2) ,
      //         pills[i].direction
      //      );
    }
  }

  for (i = 0; i < NUM_PILLS; i++)  
  {
      pills[i].speed = 1;                      // set to 1, values 2, 5            
  }

}


void handle_pills()
{
  int i, j, pill_eaten, active_pills;
  int cell_nr_x, cell_nr_y, cell_x_pill, cell_y_pill;
  
  active_pills = 0;
  for (i = 0; i < NUM_PILLS; i++) {
         if (pills[i].status != 0) {
             active_pills++;
             //printf("Active pills: %d\n", active_pills);
          }   
  }        

  /* increase speed of pills if number of pills less than 6 */
  if (active_pills >= 2 && active_pills < NUM_PILLS/2) {
     for (i = 0; i < NUM_PILLS; i++) {  // search active pill
        if (pills[i].status != 0) {     
               cell_nr_x = ( (pills[i].x / factor) - (9 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
               cell_nr_y = ( (pills[i].y / factor) - (7 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  

               cell_x_pill = (MAZE_OFFSET_X  + 9 + cell_nr_x * 20) * factor;
               cell_y_pill = (MAZE_OFFSET_Y  + 7 + cell_nr_y * 14) * factor;

               if (cell_x_pill == pills[i].x && cell_y_pill == pills[i].y) { // pill exactly in middle of cell
                    pills[i].speed = 2;  
               }
            }    
          }        // status != 0
        }       
      
  // increase speed of last pill to speed of munchkin 
  if (active_pills == 1) {
     for (i = 0; i < NUM_PILLS; i++) {  // search active pill
        if (pills[i].status != 0) {     
            if (last_pill_speed_increased == 0) {  // increase only once
 
               cell_nr_x = ( (pills[i].x / factor) - (9 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
               cell_nr_y = ( (pills[i].y / factor) - (7 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  

               cell_x_pill = (MAZE_OFFSET_X  + 9 + cell_nr_x * 20) * factor;
               cell_y_pill = (MAZE_OFFSET_Y + 7 + cell_nr_y * 14) * factor;

               if (cell_x_pill == pills[i].x && cell_y_pill == pills[i].y) { // pill exactly in middle of cell
                    //printf("One pill left, speed increased!\n");
                    pills[i].speed = speed;             // speed same as munchkin
                    last_pill_speed_increased = 1;
               }
            }      // increase once
          }        // status != 0
        }       
      }

  for (i = 0; i < NUM_PILLS && maze_completed == FALSE; i++) {

     if (pills[i].status != 0) {   // active

         pill_eaten == FALSE; 
         if (munchkin_dying == FALSE ) pill_eaten = check_pill_eaten(i);  // to prevent eating when dying

         if (pill_eaten == FALSE) {
             active_pills ++;

             choose_pill_direction(i);

             switch (pills[i].direction) {

               case 1:    //left
                    pills[i].x = pills[i].x - pills[i].speed;
                    //if ( (pills[i].x / factor) < 2) pills[i].x = 196 * factor ;  // wrap screen left
                    if ( (pills[i].x / factor) < (MAZE_OFFSET_X - 7)) pills[i].x = (187 + MAZE_OFFSET_X) * factor ;  // wrap screen left
                  break;
               case 2:    //right
                    pills[i].x = pills[i].x + pills[i].speed;
                    // 187 = cell 9 * (HORI_LINE_SIZE - 2) + pill x offset = 9*20 + 9 -> correctie -2
                    if ( (pills[i].x / factor ) > (187 + MAZE_OFFSET_X) ) pills[i].x = (MAZE_OFFSET_X - 7) * factor;  // wrap screen left
                  break;
               case 3:    //up
                    pills[i].y = pills[i].y - pills[i].speed;
                  break;
               case 4:    //down
                    pills[i].y = pills[i].y + pills[i].speed;
                   break;
              }   // end switch

            } else {   // pill_eaten
                 if (munchkin_dying == FALSE) {
                   if (pills[i].status == 1) play_sound(12,2);
                       else play_sound(14,4);
                 }    
                 /* increase score and change ghost status if powerpill */
                 if (pills[i].status == 1) score++;

                 if (pills[i].status == 2) {  // powerpill
                      score = score + 3;   
                      for (j = 0; j < NUM_GHOSTS; j++) {  // loop active ghosts
                         if (ghosts[j].status == 1 || ghosts[j].status == 2) {   // can still be 2
                              ghosts[j].status = 2;       // ghost can be eaten now
                              powerpill_active_timer = 180;
                         }
                      }
                 }     

                 if (score > high_score) {
                      high_score = score;
                      high_score_broken = TRUE;
                 }      
                 pills[i].status = 0;   

                 // count active pills left
                 active_pills = 0;
                 for (j = 0; j < NUM_PILLS; j++) {
                      if (pills[j].status != 0) {
                          active_pills ++;
                      }    
                 }

            }        // end pill not eaten
      }              // if active
    }                // for loop

    if (active_pills == 0 && maze_completed == FALSE) {
         //printf("Maze completed\n");
         maze_completed = TRUE;
         maze_completed_animations = 75;  // +/-  3 seconds
         play_sound(13, 3);
    }

}


int check_pill_eaten(int i)
{
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right munchkin
  int b_x, b_y, b_xr, b_yb;       // top-left and bottom-right pill

  a_xr = (pills[i].x + 3 * factor);   // width  factor pixel
  a_yb = (pills[i].y + 2 * factor);   // height factor pixel
  a_x  = pills[i].x;
  a_y  = pills[i].y;
 
  // make munchkin dection area smaller to give the impressin that 
  // the pill is really eaten (ie pill detecten in center of munchkin)
  b_xr = (munchkin_x + 4 * factor);    // width  factor pixel
  b_yb = (munchkin_y + 4 * factor);    // height factor pixel
  b_x  = munchkin_x + 2 * factor;
  b_y  = munchkin_y + 2 * factor;

  /* check overlap  */
  if (b_xr  > a_x   &&
      b_x   < a_xr  &&
      b_yb  > a_y   &&
      b_y   < a_yb) {
      //printf("Pill %d eaten !\n", i);
      return(TRUE);
   } else {
      return(FALSE);
   }   
}


void choose_pill_direction (int i)
{
  int cell_nr_x, cell_nr_y, cell_x_pill, cell_y_pill;
  int left_open, right_open, up_open, down_open;     //1=open, 0=closed
  int found;

  cell_nr_x = ( (pills[i].x / factor) - (9 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  cell_nr_y = ( (pills[i].y / factor) - (7 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  

  cell_x_pill = (MAZE_OFFSET_X  + 9 + cell_nr_x * 20) * factor;
  cell_y_pill = (MAZE_OFFSET_Y  + 7 + cell_nr_y * 14) * factor;

  if (cell_x_pill == pills[i].x && cell_y_pill == pills[i].y) { // pill exactly in middle of cell
     
     //printf("Pill %d (direction %d) exact op cell %d - %d\n",i, pills[i].direction, cell_nr_x, cell_nr_y);

     // determine available directions
     if (vertical_lines[cell_nr_y].line[cell_nr_x] == '|')       left_open  = 0; else left_open = 1;
     if (vertical_lines[cell_nr_y].line[cell_nr_x + 1] == '|')   right_open = 0; else right_open = 1;
     if (horizontal_lines[cell_nr_y].line[cell_nr_x] == 'x')     up_open = 0;    else up_open = 1;
     if (horizontal_lines[cell_nr_y + 1].line[cell_nr_x] == 'x') down_open = 0;  else down_open = 1;

     // do not choose center cell
     if (cell_nr_y == 4 && cell_nr_x == 3)     right_open  = 0;
     if (cell_nr_y == 4 && cell_nr_x == 5)     left_open  = 0;
     if (cell_nr_y == 5 && cell_nr_x == 4)     up_open  = 0;
     if (cell_nr_y == 3 && cell_nr_x == 4)     down_open  = 0;


     switch (pills[i].direction) {
       case 1:  //left
          // continue left (70% chance, else go up or down)
          //    if not, go right back (return)
          if (left_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = 1;
          } else {
                   if (up_open == 1 || down_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go up or down, if possible
                          if (rand()%2 == 0 && up_open == 1) {
                              found = 1;
                              pills[i].direction = 3;
                          } else { 
                                   if (down_open == 1) {
                                       found = 1;      
                                       pills[i].direction = 4;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (left_open == 1) {
                                pills[i].direction = 1;
                            } else {    
                                pills[i].direction = 2;
                            } 
                    }
          }                 
        break;
      case 2: //right
          // continue right (70% chance, else go up or down)
          //    if not, go left back (return)
          if (right_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = 2;
          } else {
                   if (up_open == 1 || down_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go up or down, if possible
                          if (rand()%2 == 0 && up_open == 1) {
                              found = 1;
                              pills[i].direction = 3;
                          } else { 
                                   if (down_open == 1) {
                                       found = 1;      
                                       pills[i].direction = 4;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (right_open == 1) {
                                pills[i].direction = 2;
                            } else {    
                                pills[i].direction = 1;
                            } 
                    }
          }                 
        break;
      case 3: //up
          // continue up (70% chance, else go left or right)
          //    if not, go right down (return)
          if (up_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = 3;
          } else {
                   if (left_open == 1 || right_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go left or right, if possible
                          if (rand()%2 == 0 && left_open == 1) {
                              found = 1;
                              pills[i].direction = 1;
                          } else { 
                                   if (right_open == 1) {
                                       found = 1;      
                                       pills[i].direction = 2;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (up_open == 1) {
                                pills[i].direction = 3;
                            } else {    
                                pills[i].direction = 4;
                            } 
                    }
          }                 
        break;
      case 4: //down   
          // continue down (70% chance, else go left or right)
          //    if not, go right up (return)
          if (down_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = 4;
          } else {
                   if (left_open == 1 || right_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go left or right, if possible
                          if (rand()%2 == 0 && left_open == 1) {
                              found = 1;
                              pills[i].direction = 1;
                          } else { 
                                   if (right_open == 1) {
                                       found = 1;      
                                       pills[i].direction = 2;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (down_open == 1) {
                                pills[i].direction = 4;
                            } else {    
                                pills[i].direction = 3;
                            } 
                    }
          }                 
        break;
     }
  }  // if middle of cell
}



void draw_pills()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  // determine next powerpill color
  if (frame % 20 == 0) powerpill_color++;
  if (powerpill_color == 5) powerpill_color = 1;  // wrap 


  for (i = 0; i < NUM_PILLS; i++)
  {
    if (pills[i].status != 0) {
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = 3 * factor;   // factor pixel
      src_rect.h = 2 * factor;   // factor pixel

      rect.x = pills[i].x;
      rect.y = pills[i].y;
      rect.w = 3;                // ignored!
      rect.h = 2;                // ignored!
      
      if (pills[i].status == 1) {
           SDL_BlitSurface(images[74], &src_rect, screen, &rect); 
      } else {
           if (frame % 20 == 0 ) {   // flash pill
              src_rect.w = 6 * factor;       // factor pixel
              src_rect.h = 5 * factor;       // factor pixel
              rect.x = pills[i].x - 1 * factor;
              rect.y = pills[i].y - 1 * factor;
              SDL_BlitSurface(images[82 + powerpill_color], &src_rect, screen, &rect);  // powerpill flash    
           } else {
           SDL_BlitSurface(images[78 + powerpill_color], &src_rect, screen, &rect);  // powerpill
           }
      }
      }   // if pill alive

   }     // for loop
}


void check_ghosts_hits_munchkin()
{
  int i, k, found;
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right coordinates of ghost
  ;
  if (munchkin_dying == FALSE) {
     /* check if munchkin collides with a ghost while ghosts is active or can be eaten */
     
     /* loop active ghosts */
     for (i = 0; i < NUM_GHOSTS; i++)
     {
       if (ghosts[i].status == 1 || ghosts[i].status == 2) {
          a_xr = (ghosts[i].x + 8 * factor) ;   // width  factor pixel
          a_yb = (ghosts[i].y + 8 * factor) ;   // height factor pixel
          a_x  = ghosts[i].x;
          a_y  = ghosts[i].y;

          if ( (munchkin_x + 6 * factor)        > a_x   &&
                munchkin_x + 2 * factor          < a_xr  &&
               (munchkin_y + 6 * factor)        > a_y   &&
                munchkin_y + 2 * factor          < a_yb) {
               if (ghosts[i].status == 1) {
                     //printf("%d - DEADLY COLLISION!\n", frame);
                     munchkin_dying = TRUE;
                     munchkin_dying_animation = 1;
                     play_sound(17, 7); 
               } else {   // ghost has status 2 and can be eaten
                     play_sound(15, 5); 
                     ghosts[i].status = 3;
                     score = score + 10;
                     if (score > high_score) {
                         high_score = score;
                         high_score_broken = TRUE;
                     }    
               }  
          }
     } // end status = 1 or 2
    }  // end loop active ghosts
  }    // munchkin_dying = FALSE
}



void setup_ghosts()
{
  int i;

  /* start position in center cell (4,4)  offset like munchkin */

  // ghost_x = (89 + 7) * factor;  // 9 + 4*20 + 7 = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2) + 7) * factor
  // ghost_y = (79 + 4) * factor;  //23 + 4*14 + 4 = (MAZE_OFFSET_Y + 4 * (VERT_LINE_SIZE -2) + 4) * factor

  for (i = 0; i < NUM_GHOSTS; i++)  {
       ghosts[i].colour = (i % 4) + 1;
       ghosts[i].status = 1;
       ghosts[i].recharge_timer = 0;
       ghosts[i].x = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2) + 7) * factor;
       ghosts[i].y = (MAZE_OFFSET_Y + 4 * (VERT_LINE_SIZE -2) + 4) * factor;
       ghosts[i].direction = DOWN;
       ghosts[i].speed = speed;  // same speed as munchkin
   }   

}


void handle_ghosts()
{
  int i;

  powerpill_active_timer --;

  if (powerpill_active_timer == 0) {   // timer completed, put ghosts to active
        for (i = 0; i < NUM_GHOSTS && maze_completed == FALSE; i++) {
             if (ghosts[i].status == 2) {   // can be eaten 
                  ghosts[i].status = 1;
             }     
        }  
  }

  for (i = 0; i < NUM_GHOSTS && maze_completed == FALSE; i++) {


             choose_ghost_direction(i);

             switch (ghosts[i].direction) {
               case LEFT:   
                    ghosts[i].x = ghosts[i].x - ghosts[i].speed;
                    if (ghosts[i].x < -10) ghosts[i].x = 970;  // wrap screen left
                  break;
               case RIGHT:   
                    ghosts[i].x = ghosts[i].x + ghosts[i].speed;
                    if (ghosts[i].x > 970) ghosts[i].x = -10;  // wrap screen left
                  break;
               case UP:   
                    ghosts[i].y = ghosts[i].y - ghosts[i].speed;
                  break;
               case DOWN:    
                    ghosts[i].y = ghosts[i].y + ghosts[i].speed;
                   break;
              }   // end switch
    }                // for loop
}


void choose_ghost_direction (int i)
{
  int cell_nr_x, cell_nr_y, cell_x_ghost, cell_y_ghost;
  int left_open, right_open, up_open, down_open;     //1=open, 0=closed
  int direction_to_center_set;
  int found;

  cell_nr_x = ( (ghosts[i].x / factor) - (7 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  cell_nr_y = ( (ghosts[i].y / factor) - (4 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  
  //printf("Ghost %d cell x %d, cell y %d\n",i, cell_nr_x , cell_nr_y);

  cell_x_ghost = (9  + 7 + cell_nr_x * 20) * factor;
  cell_y_ghost = (23 + 4 + cell_nr_y * 14) * factor;

  direction_to_center_set = FALSE;     // for ghosts with status 3, going to center

  //printf("Pill %d (direction %d) x=%d y=%d\n", i, pills[i].direction, pills[i].x, pills[i].y);


  if (cell_x_ghost == ghosts[i].x && cell_y_ghost == ghosts[i].y) { // ghost exactly in middle of cell
     
     //printf("Pill %d (direction %d) exact op cell %d - %d\n",i, pills[i].direction, cell_nr_x, cell_nr_y);

     // determine available directions
     if (vertical_lines[cell_nr_y].line[cell_nr_x] == '|')       left_open  = 0; else left_open = 1;
     if (vertical_lines[cell_nr_y].line[cell_nr_x + 1] == '|')   right_open = 0; else right_open = 1;
     if (horizontal_lines[cell_nr_y].line[cell_nr_x] == 'x')     up_open = 0;    else up_open = 1;
     if (horizontal_lines[cell_nr_y + 1].line[cell_nr_x] == 'x') down_open = 0;  else down_open = 1;

     // printf("--- open Left %d Right %d Up %d Down %d\n\n",left_open, right_open, up_open, down_open);


     if (ghosts[i].status == 3) {  // eaten, looking for center
         if (cell_nr_x == 4 && cell_nr_y == 4) {
             //printf("Ghost %d reached center, going to recharge\n",i);
             ghosts[i].status = 4;
             ghosts[i].direction = 0;
             ghosts[i].recharge_timer = 200; 

           } else {  // try to move into center if nearby

             /* check movement to center when at cell (3,4) */
             if (cell_nr_x == 3 && cell_nr_y == 4 && right_open == 1) {
                //printf("Ghost %d can go right into center\n",i);
                ghosts[i].direction = RIGHT;
                direction_to_center_set = TRUE;
             }
             /* stay around cell (3,4) if center was not open */
             if (cell_nr_x == 3 && cell_nr_y == 4 && direction_to_center_set == FALSE) {
                //printf("Ghost %d cannot go right into center\n",i);
                if (ghosts[i].direction == UP && up_open == 1)     ghosts[i].direction = UP;    // continue up
                if (ghosts[i].direction == UP && up_open == 0 && left_open == 1)   ghosts[i].direction = LEFT;   // NEW
                if (ghosts[i].direction == UP && up_open == 0 && down_open == 1)   ghosts[i].direction = DOWN;   // NEW
                if (ghosts[i].direction == DOWN && down_open == 1) ghosts[i].direction = DOWN;  // continue down
                if (ghosts[i].direction == DOWN && down_open == 0 && left_open == 1) ghosts[i].direction = LEFT;  // NEW
                if (ghosts[i].direction == DOWN && down_open == 0 && left_open == 0) ghosts[i].direction = UP;  // NEW
                if (ghosts[i].direction == RIGHT && up_open == 1)  ghosts[i].direction = UP; 
                if (ghosts[i].direction == RIGHT && up_open == 0 && down_open == 1) 
                      ghosts[i].direction = DOWN; 
                if (ghosts[i].direction == RIGHT && up_open == 0 && down_open == 0) 
                      ghosts[i].direction = LEFT;  // go back
                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (5,4) */
             /* (a bit ugly, sorry) */

             if (cell_nr_x == 5 && cell_nr_y == 4 && left_open == 1) {
              //printf("Ghost %d can go left into center\n",i);
              ghosts[i].direction = LEFT;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (5,4) if center was not open */
             if (cell_nr_x == 5 && cell_nr_y == 4 && direction_to_center_set == FALSE) {
                //printf("Ghost %d cannot go left into center\n",i);
                if (ghosts[i].direction == UP && up_open == 1)     ghosts[i].direction = UP;    // continue up
                if (ghosts[i].direction == UP && up_open == 0 && right_open == 1)     ghosts[i].direction = RIGHT;    // NEW
                if (ghosts[i].direction == UP && up_open == 0 && right_open == 0)     ghosts[i].direction = DOWN;    // NEW
                if (ghosts[i].direction == DOWN && down_open == 1) ghosts[i].direction = DOWN;  // continue down
                if (ghosts[i].direction == DOWN && down_open == 0 && right_open == 1) ghosts[i].direction = RIGHT;  // NEW
                if (ghosts[i].direction == DOWN && down_open == 0 && right_open == 0) ghosts[i].direction = UP;  // NEW
                if (ghosts[i].direction == LEFT && up_open == 1)   ghosts[i].direction = UP; 
                if (ghosts[i].direction == LEFT && up_open == 0 && down_open == 1) 
                      ghosts[i].direction = DOWN; 
                if (ghosts[i].direction == LEFT && up_open == 0 && down_open == 0) 
                      ghosts[i].direction = RIGHT;  // go back
                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (4,3) */
             if (cell_nr_x == 4 && cell_nr_y == 3 && down_open == 1) {
              //printf("Ghost %d can go down into center\n",i);
              ghosts[i].direction = DOWN;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (4,3) if center was not open */
             if (cell_nr_x == 4 && cell_nr_y == 3 && direction_to_center_set == FALSE) {
                //printf("Ghost %d cannot go down into center\n",i);
                if (ghosts[i].direction == LEFT && left_open == 1)   ghosts[i].direction = LEFT;   // continue left
                if (ghosts[i].direction == LEFT && left_open == 0 && up_open == 1)   ghosts[i].direction = UP;   // NEW
                if (ghosts[i].direction == LEFT && left_open == 0 && up_open == 0)   ghosts[i].direction = RIGHT;   // NEW
                if (ghosts[i].direction == RIGHT && right_open == 1) ghosts[i].direction = RIGHT;  // continue right
                if (ghosts[i].direction == RIGHT && right_open == 0 && up_open == 1) ghosts[i].direction = UP;  // NEW
                if (ghosts[i].direction == RIGHT && right_open == 0 && up_open == 0) ghosts[i].direction = LEFT;  // NEW
                if (ghosts[i].direction == DOWN && left_open == 1)   ghosts[i].direction = LEFT; 
                if (ghosts[i].direction == DOWN && left_open == 0 && right_open == 1) 
                      ghosts[i].direction = RIGHT; 
                if (ghosts[i].direction == DOWN && left_open == 0 && right_open == 0) 
                      ghosts[i].direction = UP;  // go back
                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (4,5) */
             if (cell_nr_x == 4 && cell_nr_y == 5 && up_open == 1) {
              //printf("Ghost %d can go up into center\n",i);
              ghosts[i].direction = UP;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (4,5) if center was not open */
             if (cell_nr_x == 4 && cell_nr_y == 5 && direction_to_center_set == FALSE) {
                //printf("Ghost %d cannot go up into center\n",i);
                if (ghosts[i].direction == LEFT && left_open == 1)   ghosts[i].direction = LEFT;   // continue left
                if (ghosts[i].direction == LEFT && left_open == 0 && down_open == 1)   ghosts[i].direction = DOWN;   // NEW
                if (ghosts[i].direction == LEFT && left_open == 0 && down_open == 0)   ghosts[i].direction = RIGHT;   // NEW                
                if (ghosts[i].direction == RIGHT && right_open == 1) ghosts[i].direction = RIGHT;  // continue right
                if (ghosts[i].direction == RIGHT && right_open == 0 && down_open == 1) ghosts[i].direction = DOWN;  // NEW
                if (ghosts[i].direction == RIGHT && right_open == 0 && down_open == 0) ghosts[i].direction = LEFT;  // NEW
                if (ghosts[i].direction == UP && left_open == 1)     ghosts[i].direction = LEFT; 
                if (ghosts[i].direction == UP && left_open == 0 && right_open == 1) 
                      ghosts[i].direction = RIGHT; 
                if (ghosts[i].direction == UP && left_open == 0 && right_open == 0) 
                      ghosts[i].direction = DOWN;  // go back
                direction_to_center_set = TRUE;
             }



           }
     } 

     if (ghosts[i].status == 4) {  // recharging
             ghosts[i].recharge_timer --;
             if (ghosts[i].recharge_timer == -1) {
                 //printf("Ghost %d recharged, become normal\n",i);
                 ghosts[i].status = 1;
                 ghosts[i].recharge_timer = 0;
                 ghosts[i].direction = DOWN;    // but others directions are possible later on
             }
      }      

     if (direction_to_center_set == FALSE) {   // direction not already set for status = 3

          switch (ghosts[i].direction) {

          case LEFT:  
             // continue left (50% chance, else go up or down)
             //    if not, go right back (return)
             if (left_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = LEFT;
             } else {
                      if (up_open == 1 || down_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go up or down, if possible
                             if (rand()%2 == 0 && up_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = UP;
                             } else { 
                                      if (down_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = DOWN;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (left_open == 1) {
                                   ghosts[i].direction = LEFT;
                               } else {    
                                   ghosts[i].direction = RIGHT;
                               } 
                       }
             }                 
           break;

         case RIGHT: 
             // continue right (50% chance, else go up or down)
             //    if not, go left back (return)
             if (right_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = RIGHT;
             } else {
                      if (up_open == 1 || down_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go up or down, if possible
                             if (rand()%2 == 0 && up_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = UP;
                             } else { 
                                      if (down_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = DOWN;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (right_open == 1) {
                                   ghosts[i].direction = RIGHT;
                               } else {    
                                   ghosts[i].direction = LEFT;
                               } 
                       }
             }                 
           break;

         case UP:
             // continue up (50% chance, else go left or right)
             //    if not, go right down (return)
             if (up_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = UP;
             } else {
                      if (left_open == 1 || right_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go left or right, if possible
                             if (rand()%2 == 0 && left_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = LEFT;
                             } else { 
                                      if (right_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = RIGHT;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (up_open == 1) {
                                   ghosts[i].direction = UP;
                               } else {    
                                   ghosts[i].direction = DOWN;
                               } 
                       }
             }                 
           break;

         case DOWN: 
             // continue down (50% chance, else go left or right)
             //    if not, go right up (return)
             if (down_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = DOWN;
             } else {
                      if (left_open == 1 || right_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go left or right, if possible
                             if (rand()%2 == 0 && left_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = LEFT;
                             } else { 
                                      if (right_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = RIGHT;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (down_open == 1) {
                                   ghosts[i].direction = DOWN;
                               } else {    
                                   ghosts[i].direction = UP;
                               } 
                       }
             }                 
           break;
         }   // end switch( direction )
     }       // end directiion_to_center_set     
  }          // if middle of cell
}


void draw_ghosts()
{

int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)
  int colour;
  int direction_image_nr;  // needed for ghosts with status 4

  for (i = 0; i < NUM_GHOSTS; i++)
  {
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = 8 * factor;   // factor pixel
      src_rect.h = 8 * factor;   // factor pixel

      rect.x = ghosts[i].x;
      rect.y = ghosts[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!
      
      if (ghosts[i].status == 1) {  // normal
           if (frame % 6 >= 0 && frame % 6 <=2 )   // move "feet" of ghosts every 6 frames
             SDL_BlitSurface(images[((ghosts[i].colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2],
                 &src_rect, screen, &rect); 
           //                                               8 for colour
           else
             SDL_BlitSurface(images[((ghosts[i].colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2 + 1],
                 &src_rect, screen, &rect); 

           //
      }  // if status =1

      if (ghosts[i].status == 2) {   // can be eaten
          colour = 5;  // magenta
          //printf("-- frame %d ghost %d flashing drawn\n", frame, i);
          if (powerpill_active_timer > 60) {  // magenta
             if (frame % 6 >= 0 && frame % 6 <=2 ) {   // move "feet" of ghosts every 6 frames
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2],
                   &src_rect, screen, &rect); 
             } else {
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2 + 1],
                   &src_rect, screen, &rect); 
             }  
          } else {  // flash magenta 5/cyan 4
             //printf("timer <= 60\n");  
             if (frame % 10 >= 0 && frame % 10 < 5 ) { colour = 5; } else { colour = 4; }
             if (frame % 6 >= 0 && frame % 6 <=2 ) {  // move "feet" of ghosts every 6 frames
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2],
                   &src_rect, screen, &rect); 
             } else {
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (ghosts[i].direction -1)*2 + 1],
                   &src_rect, screen, &rect); 
             }

          }  // timer > 30  
       }   // if status =2


       if (ghosts[i].status == 3 || ghosts[i].status == 4 ) {   // eaten or recharging
          direction_image_nr = ghosts[i].direction;
          // alternate between white and invisible 
          if (ghosts[i].status == 4) {
              //printf("Ghost %d in center, frame %d, status \n", i, frame, ghosts[i].status);
              direction_image_nr = 3;  // if ghost in center, direction = 0, so pretend
                                       //   ghost is looking up for drawing in center
          }    
          if (frame % 20 >= 0 && frame % 20 < 14 ) { colour = 7; } else { colour = 6; }
             if (frame % 6 >= 0 && frame % 6 <=2 )   // move "feet" of ghosts every 6 frames
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (direction_image_nr -1)*2],
                   &src_rect, screen, &rect); 
             //                                               8 for colour
             else
               SDL_BlitSurface(images[((colour - 1) * 8) + 87 + (direction_image_nr -1)*2 + 1],
                   &src_rect, screen, &rect); 
       }   // if status = 3 or 4

   }     // for loop  
}



void draw_score_line() 
{
  SDL_Color fgColor_green  = {0,182,0};   
  SDL_Color fgColor_red    = {182,0,0};   
  SDL_Color fgColor_grey   = {182,182,182};   // grey/white
  SDL_Rect text_position;  
  char text_line[20]; 

  // highscore in green
  sprintf(text_line, "%04d", high_score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = 24 * factor;
  text_position.y = (145-20) * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // arrow sign in grey/white
  sprintf(text_line, "%c", 124); // arrow-char in modified o2 font
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_grey);
  text_position.x = (24 * factor) + (3 * 12 * factor); // skip 3 chars
  text_position.y = (145-20) * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // highscore name in green
  if (munchkin_dying == 1) {
    flash_high_score_name();
  } else {  
    sprintf(text_line, "%s ", high_score_name);
    text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
    text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
    text_position.y = (145-20) * factor;
    SDL_BlitSurface(text, NULL , screen, &text_position);
  }

  // current score in red
  sprintf(text_line, " %04d", score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  text_position.x = (24 * factor) + (9 * 12 * factor); // skip 9 chars
  text_position.y = (145-20) * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

}


void flash_high_score_name()
{
  SDL_Color fgColor_green  = {0,182,0};   
  SDL_Rect text_position;  
  char text_line[6];

  if (high_score_broken == TRUE) strcpy(high_score_name, "??????");

  // highscore name in green
  sprintf(text_line, "%s ", high_score_name);
  
  text_line[flash_high_score_timer%6] = ' ';
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
  text_position.y = (145-20) * factor;
  
  SDL_BlitSurface(text, NULL , screen, &text_position);
  
  if (frame%3 == 0) {
    flash_high_score_timer--;
  }  
  if (flash_high_score_timer < 0) flash_high_score_timer = 150;
}


void print_high_score_char(int character)
{
  SDL_Color fgColor_green = {0,182,0};   
  SDL_Color fgColor_red    = {182,0,0};   
  SDL_Rect text_position;  
  char text_line[6];
  
  // highscore name in green
  strcpy(text_line, high_score_name);
  
  if (character != 13) text_line[high_score_character_pos] = character;
  strcpy(high_score_name, text_line);

  high_score_character_pos++;
  if (high_score_character_pos > 5 || character == 13) {  // max length or return
    high_score_registration = FALSE;
    //printf("stop registration\n");
  }
    
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
  text_position.y = (145-20) * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // current score in red
  sprintf(text_line, " %04d", score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  text_position.x = (24 * factor) + (9 * 12 * factor); // skip 9 chars
  text_position.y = (145-20) * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  play_sound(7, -1);
}


void play_sound(int snd, int chan)
{
   /* sounds:
         0 : not used
         1 : not used
         2 : not used
         3 : not used
         4 : not used
         5 : not used
         6 : score              
         7 : character beep (on free channel
         8 : not used
         9 : not used
        10 : select game (only in intro on free channel)  
        11 : munchkin walk      channel 1
        12 : munchkin eat pill  channel 2
        13 : munchkin maze completed channel 3/  
        14 : munchkin eat powerpill  channel 4
        15 : munchkin ghost eaten channel 5 
        16 : muchkin ghost move sound channel 6
        17 : munchkin dying sound channel 7 */

    // Some tweaks to improve sounds (SDL_Mixer is not perfect)

    if (snd == 7) chan = Mix_PlayChannel(chan, sounds[snd], 0);

    if (snd == 10) {
       if (Mix_Playing(7) != 0) Mix_HaltChannel(7);  // stop munchkin die sound;
       if (Mix_Playing(3) != 0) Mix_HaltChannel(3);  // stop munchkin move sound
       if (Mix_Playing(6) != 0) Mix_HaltChannel(6);  // stop ghosts move sound
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
    }  

    if (snd == 11 && chan == 1) {
       if (Mix_Playing(6) != 0) Mix_HaltChannel(6);  // stop ghosts move sound;
       if (Mix_Playing(chan) != 0) {
            ; //printf("channel %d skiiped\n", chan);
       } else {
            //printf("channel %d played\n", chan);
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       }    
    }   

    if (snd == 12 && chan == 2) {
       if (Mix_Playing(chan) != 0) {
            Mix_HaltChannel(chan); 
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       } else {
            //printf("channel %d played\n", chan);
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       }    
    }   

    if (snd == 13 && chan == 3) {
       if (Mix_Playing(6) != 0)  Mix_HaltChannel(6);  // stop ghosts move sound
       if (Mix_Playing(1) != 0) Mix_HaltChannel(1);   // halt movement sound
       chan = Mix_PlayChannel(chan, sounds[snd], 0);
    }   

    if (snd == 14 && chan == 4) {
       if (Mix_Playing(chan) != 0) {
            Mix_HaltChannel(chan); 
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       } else {
            //printf("channel %d played\n", chan);
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       }    
    }   

    if (snd == 15 && chan == 5) {
       if (Mix_Playing(1) != 0) Mix_HaltChannel(1);        // halt munchkin movement sound
       //printf("eaten sound\n");
       chan = Mix_PlayChannel(chan, sounds[snd], 0);
    }  

    if (snd == 16 && chan == 6) {
       if (Mix_Playing(1) != 0)  Mix_HaltChannel(1);  // stop munchkin move sound
       if (Mix_Playing(chan) != 0) {
            ; //printf("channel %d skiiped\n", chan);
       } else {
            //printf("channel %d played\n", chan);
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
       }    
    }   

    if (snd == 17 && chan == 7) {
       if (Mix_Playing(6) != 0) Mix_HaltChannel(6);  // stop ghosts move sound;
       if (Mix_Playing(1) != 0) Mix_HaltChannel(1);  // stop munchkin move sound
            chan = Mix_PlayChannel(chan, sounds[snd], 0);
    }    

}


void title_screen()
{
  int done, x, y, ux, uy;
  int window_size_changed;
  int scroll_x;
  Uint32 last_time;
  SDL_Event event;
  SDLKey key;
  char title_string[100];
  int active_option_row;

  /* title screen loop */
  start_delay = frame;
  done = 0;
  play_sound(10,-1);    // select game 

  x = (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor);  // center - 5 characters
                                                      // (large font is 12 pixels char)
  y = (VIDEOPAC_RES_H / 2 * factor) - (5 * factor);

  //joy_up = FALSE; joy_down = FALSE; joy_left = FALSE; joy_right = FALSE;
  active_option_row = 1;
  scroll_x = 0;
  powerpill_color = 1;
  switch_active_mini_map(maze_selected);

  do
  {
    last_time = SDL_GetTicks();
      
    /* Check for key presses and joystick actions */
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_JOYBUTTONDOWN  && (event.jbutton.button == 0 ||
                                               event.jbutton.button == 1)) {     
        //printf("Joystick fire button A or B pressed, start normal game\n");      
        sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                              , maze_selected, NUM_GHOSTS, NUM_PILLS);
        SDL_WM_SetCaption(title_string, "MUNCHKIN");  
        done = 1;
      }   

      if (event.type == SDL_JOYAXISMOTION ) {  // prevent many actions at once
        // check if joystick 0 has moved
        if ( event.jaxis.which == 0 ) {
            //If the X axis changed
            if ( event.jaxis.axis == 0 ) {
                //If the X axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_left = 0;
                     joy_right = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                        joy_left = 1;
                        joy_right = 0;
                    } else {
                        joy_left = 0;
                        joy_right = 1;
                    }
                }
            }
            //If the Y axis changed
            else if ( event.jaxis.axis == 1 ) {
                //If the Y axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_up = 0;
                     joy_down = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                     joy_up = 1;
                     joy_down = 0;
                    } else {
                        joy_up = 0;
                        joy_down = 1;
                    }
                }
            }
        }   // if event.jaxis.which == 0   
      }  // event.type == SDL_JOYAXISMOTION

        //printf("last_joystick_action: %d LRUP %d %d %d %d \n", last_joystick_action, 
        //            joy_left, joy_right, joy_up, joy_down);
        // check joystick action (process only one action at a time)

        if (last_joystick_action == TRUE &&
            joy_up == 0 && joy_down == 0 && joy_left == 0 && joy_right == 0 ) {        
              last_joystick_action = FALSE;
              //printf("--- switch to FALSE \n");
        }    


        if (last_joystick_action == FALSE && 
            (joy_up == 1 || joy_down == 1 || joy_left == 1 || joy_right == 1) ) {

             //printf("--- switch to TRUE \n");
             last_joystick_action = TRUE;
             if (joy_up == 1 || joy_down == 1 || joy_left == 1 || joy_right == 1) {  // select just 1 movement
                 if (joy_up == 1)   if (active_option_row > 1) active_option_row --;
                 if (joy_down == 1) if (active_option_row < 4) active_option_row ++;
                 
                 if (joy_right == 1) {  
                   switch (active_option_row) {
                     case(1):  // maze option
                        maze_selected++;
                        if (maze_selected == 5) maze_selected = 1;
                        switch_active_mini_map(maze_selected);
                        break;
                      case(2): // ghosts option
                        NUM_GHOSTS++;
                        if (NUM_GHOSTS > 10) NUM_GHOSTS = 10; 
                        break;
                      case(3): // pills option
                        NUM_PILLS++;
                        if (NUM_PILLS > 99) NUM_PILLS = 99; 
                        break;
                      case(4): // start option
                        sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                               ,maze_selected, NUM_GHOSTS, NUM_PILLS);
                        SDL_WM_SetCaption(title_string, "MUNCHKIN");  
                        done = 1;
                        break;
                   }  // end switch option row
                 }    // (joy_right == 1)

                 if (joy_left == 1) {
                    switch (active_option_row) {
                      case(1):  // maze option
                        maze_selected--;
                        if (maze_selected == 0) maze_selected = 4;
                        switch_active_mini_map(maze_selected);
                        break;
                      case(2): // ghosts option
                         NUM_GHOSTS--;
                         if (NUM_GHOSTS < 1) NUM_GHOSTS = 1; 
                         break;
                      case(3): // pills option
                         NUM_PILLS--;
                         if (NUM_PILLS < 12) NUM_PILLS = 12; 
                         break;
                       case(4): // start option
                         sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                                , maze_selected, NUM_GHOSTS, NUM_PILLS);
                         SDL_WM_SetCaption(title_string, "MUNCHKIN");  
                         done = 1;
                         break;
                    }  // end switch option row
                 }  // (joy_left == 1)
              }   // joy_up == 1 || joy_down == 1 || joy_left == 1 || joy_right == 1)
   } //(last_joystick_action == FALSE
          

      
      if (event.type == SDL_KEYDOWN) {
          key = event.key.keysym.sym;
            
           if (key == 49 || key == SDLK_LCTRL) {  // button 1 or Ctrl pressed
                sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                                    , maze_selected, NUM_GHOSTS, NUM_PILLS);
                SDL_WM_SetCaption(title_string, "MUNCHKIN");  
                done = 1;            
           } 

           if (key >= 273 && key <= 276) {  // handle arrowkeys
             switch (key) {
               case 273:  // up
                   if (active_option_row > 1) active_option_row --;
                   break;
               case 274:  // down
                   if (active_option_row < 4) active_option_row ++;
                   break;
               case 275:  // right
                   switch (active_option_row) {
                     case(1):  // maze option
                        maze_selected++;
                        if (maze_selected == 5) maze_selected = 1;
                        switch_active_mini_map(maze_selected);
                        break;
                      case(2): // ghosts option
                        NUM_GHOSTS++;
                        if (NUM_GHOSTS > 10) NUM_GHOSTS = 10; 
                        break;
                      case(3): // pills option
                        NUM_PILLS++;
                        if (NUM_PILLS > 99) NUM_PILLS = 99; 
                        break;
                      case(4): // start option
                        sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                               ,maze_selected, NUM_GHOSTS, NUM_PILLS);
                        SDL_WM_SetCaption(title_string, "MUNCHKIN");  
                        done = 1;
                        break;
                   }  // end switch option row
                   break;
              case 276:  // left
                   switch (active_option_row) {
                     case(1):  // maze option
                       maze_selected--;
                       if (maze_selected == 0) maze_selected = 4;
                       switch_active_mini_map(maze_selected);
                       break;
                     case(2): // ghosts option
                        NUM_GHOSTS--;
                        if (NUM_GHOSTS < 1) NUM_GHOSTS = 1; 
                        break;
                     case(3): // pills option
                        NUM_PILLS--;
                        if (NUM_PILLS < 12) NUM_PILLS = 12; 
                        break;
                      case(4): // start option
                        sprintf(title_string, "Munchkin - maze: %d - ghosts: %d - pills: %d"
                               , maze_selected, NUM_GHOSTS, NUM_PILLS);
                        SDL_WM_SetCaption(title_string, "MUNCHKIN");  
                        done = 1;
                        break;
                   }  // end switch option row
                   break;
              }  // end switch     

           }  // end if arrow keys


           /* key 8: Toggle full screen */
           if (key == 56) {     
             if (full_screen == FALSE) {
                screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT | SDL_FULLSCREEN);
                full_screen = TRUE;
                if (screen == NULL)
                {
                  fprintf(stderr, "\nWarning: I could not set up video for "
                          "full-screen.\n"
                          "The Simple DirectMedia error that occured was:\n"
                          "%s\n\n", SDL_GetError());
                }
             } else {
               screen_width  =  factor * VIDEOPAC_RES_W;           
               screen_height =  factor * VIDEOPAC_RES_H;
               screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
               full_screen = FALSE;
               if (screen == NULL)
               {
                  fprintf(stderr, "\nWarning: I could not set up video for "
                          "larger/smaller mode.\n"
                          "The Simple DirectMedia error that occured was:\n"
                          "%s\n\n", SDL_GetError());
                }
              }

           }  // end key = full screen

           if (key == SDLK_ESCAPE)
              exit(0);
        }
      else if (event.type == SDL_QUIT || (event.type == SDL_JOYBUTTONDOWN  && event.jbutton.button == 8) )   
                         // close window pressed or home button on joystick
        {
          exit(0);
        }
    }  // end while (SDL_PollEvent(&event))


    /* draw black screen */
    SDL_FillRect(screen, NULL,
    SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));

    frame++;
    if (frame - start_delay >= 20*3) {   
       x =  (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor); 
       y = y - factor;
       if (y < 15*factor) {
          y = y + factor;
          //display_instructions(-1 * scroll_x, 40);
          display_instructions(-1 * scroll_x, 145);
          display_active_option_row(active_option_row);
          scroll_x++;
          if (scroll_x == 269) scroll_x = 0;
       }   

    } else {  
      // wobbly text!
      x = ( (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor) + (( rand() % 4) - 2));
      y = ( (VIDEOPAC_RES_H / 2 * factor) - (5 * factor)  + (( rand() % 4) - 2));
    }
    display_select_game(x, y);

    SDL_Flip(screen);

    if (SDL_GetTicks() < last_time + 33)
        SDL_Delay(last_time + 33 - SDL_GetTicks());
      
  } // end do
  while (done == 0);
}


void display_select_game(int x, int y)
{
  SDL_Color fgColor_green   = {0,182,0};   
  SDL_Color fgColor_red     = {182,0,0};   
  SDL_Color fgColor_grey    = {182,182,182};  
  SDL_Color fgColor_yellow  = {182,182,0};  
  SDL_Color fgColor_blue    = {0,0,182};  
  SDL_Color fgColor_magenta = {182,0,182};  
  SDL_Color fgColor_cyan    = {0,182,182};  

  SDL_Rect text_position;  
  char text_line[12];    // SELECT GAME

  sprintf(text_line, "%s", "S       A  ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = x;
  text_position.y = y;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", " E       M ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_yellow);
  SDL_BlitSurface(text, NULL , screen, &text_position);
    
  sprintf(text_line, "%s", "  L       E");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_blue);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "   E       ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_magenta);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "    C      ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_cyan);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "     T     ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_grey);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "       G   ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  SDL_BlitSurface(text, NULL , screen, &text_position);
}


void display_instructions(int scroll_x, int scroll_y)
{
  SDL_Color fgColor_green   = {0,182,0};   
  SDL_Color fgColor_red     = {182,0,0};   
  SDL_Color fgColor_grey    = {182,182,182};  
  SDL_Color fgColor_yellow  = {182,182,0};  
  SDL_Color fgColor_blue    = {0,0,182};  
  SDL_Color fgColor_magenta = {182,0,182};  
  SDL_Color fgColor_cyan    = {0,182,182};  

  SDL_Rect text_position;  
  char text_line[240]; 
  //"        SELECT OPTIONS WITH ARROWS KEYS OR JOYSTICK               SELECT OPTIONS WITH ARROW KEYS OR JOYSTICK "

  sprintf(text_line, "%s", 
    "          SELECT OPTIONS                                            SELECT OPTIONS                             ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_green);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", 
    "                          WITH ARROW KEYS                                           WITH ARROW KEYS            ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_cyan);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", 
    "                                          OR JOYSTICK                                               OR JOYSTICK");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_magenta);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "GHOSTS  %02d", NUM_GHOSTS);
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_green);
  text_position.x = 80 * factor;
  text_position.y = 65 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "PILLS   %02d", NUM_PILLS);
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_magenta);
  text_position.x = 80 * factor;
  text_position.y = 75 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "START GAME");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_cyan);
  text_position.x = 80 * factor;
  text_position.y = 85 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "8 = FULL SCREEN");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_red);
  text_position.x = 35 * factor;
  text_position.y = 100 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "                  ESC = QUIT");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_grey);
  text_position.x = 30 * factor;
  text_position.y = 100 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "1 pt       3 pts       10 pts");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_yellow);
  text_position.x = 30 * factor;
  text_position.y = 130 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  /* display mini versions of mazes */
  factor = 1; 
  
  MAZE_OFFSET_X = 9 + 40;  MAZE_OFFSET_Y = 175;
  setup_maze(1);  maze_color = 'y'; 
  if (maze_selected == 1) { maze_color = 'm'; handle_pills(); draw_pills(); }
  draw_maze();
  
  MAZE_OFFSET_X = 235+9 + 40;  MAZE_OFFSET_Y = 175;
  setup_maze(2);  maze_color = 'y'; 
  if (maze_selected == 2) { maze_color = 'm'; handle_pills(); draw_pills(); }
  draw_maze();
  
  MAZE_OFFSET_X = 470+9 + 40;  MAZE_OFFSET_Y = 175;
  setup_maze(3);  maze_color = 'y'; 
  if (maze_selected == 3) { maze_color = 'm'; handle_pills(); draw_pills(); }
  draw_maze();
  
  MAZE_OFFSET_X = 705+9 + 40;  MAZE_OFFSET_Y = 175;
  setup_maze(4);  maze_color = 'y'; 
  if (maze_selected == 4) { maze_color = 'm'; handle_pills(); draw_pills(); }
  draw_maze();

  MAZE_OFFSET_X = 9;  MAZE_OFFSET_Y = 23;
  factor = 5;
  // activate mini maze for pills.
  setup_maze(maze_selected);


  /* draw pill, powerpill and ghost */

  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  // draw pill
  src_rect.x = 0;            // left
  src_rect.y = 0;            // up
  src_rect.w = 3 * factor;   // factor pixel
  src_rect.h = 2 * factor;   // factor pixel
  rect.x = 35 * 5;
  rect.y = 120 * 5;
      
  SDL_BlitSurface(images[74], &src_rect, screen, &rect); 

  src_rect.w = 6 * factor;       // factor pixel
  src_rect.h = 5 * factor;       // factor pixel
  rect.x = 90 * 5;     
  rect.y = 120 * 5;

  // determine next powerpill color
  if (frame % 20 == 0) powerpill_color++;
  if (powerpill_color == 5) powerpill_color = 1;  // wrap 


  if (frame % 20 == 0 ) {   // flash pill
      rect.x = 89 * 5;     
      rect.y = 119 * 5;
      SDL_BlitSurface(images[82 + powerpill_color], &src_rect, screen, &rect);  // powerpill flash    
  } else {
      SDL_BlitSurface(images[78 + powerpill_color], &src_rect, screen, &rect);  // powerpill
  }       

  // draw ghost

  src_rect.x = 0;            // left
  src_rect.y = 0;            // up
  src_rect.w = 8 * factor;   // factor pixel
  src_rect.h = 8 * factor;   // factor pixel

  rect.x = 145 * 5;
  rect.y = 120 * 5;
      
  if (frame % 6 >= 0 && frame % 6 <=2 )   // move "feet" of ghosts every 6 frames
      SDL_BlitSurface(images[((3) * 8) + 87], &src_rect, screen, &rect); 
  else
      SDL_BlitSurface(images[((3) * 8) + 87 + 1], &src_rect, screen, &rect); 

}  


void switch_active_mini_map(int maze_selected)
{
   factor = 1;
   switch (maze_selected) { 
    case 1:
       MAZE_OFFSET_X = 9 + 40;  MAZE_OFFSET_Y = 175;
       break;
    case 2:
       MAZE_OFFSET_X = 235+9 + 40;  MAZE_OFFSET_Y = 175;
       break;
    case 3:
       MAZE_OFFSET_X = 470+9 + 40;  MAZE_OFFSET_Y = 175;
       break;
    case 4:
       MAZE_OFFSET_X = 705+9 + 40;  MAZE_OFFSET_Y = 175;
       break;
   } // end switch maze selected

   setup_pills();  

   // back to default values
   MAZE_OFFSET_X = 9;  MAZE_OFFSET_Y = 23;
   factor = 5; 
}


void display_active_option_row(int row)
{
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  switch (row) {
    case 1:   // highlight "maze" option row
      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = 2 * 5;   
      src_rect.h = VERT_LINE_SIZE * 5;  

      rect.x = 5;
      rect.y = 182;
      if (frame % 20 >= 0 && frame % 20 < 10) {
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
          rect.x = 195 * 5;
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
      } else {
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
          rect.x = 195 * 5;
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
      }
      break; 
    case 2: // highlight "ghosts" option row
      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = 2 * 5;   
      src_rect.h = 6 * 5;  

      rect.x = 350;
      rect.y = 65 * 5 + 2;
      if (frame % 20 >= 0 && frame % 20 < 10) {
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
      } else {
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
      }
      break;         
    case 3: // highlight "pills" option row
      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = 2 * 5;   
      src_rect.h = 6 * 5;  

      rect.x = 350;
      rect.y = 75 * 5 + 2;
      if (frame % 20 >= 0 && frame % 20 < 10) {
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
      } else {
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
      }
      break;               
    case 4: // highlight "start game" option row
      src_rect.x = 0;  // left
      src_rect.y = 0;  // up
      src_rect.w = 2 * 5;   
      src_rect.h = 6 * 5;  

      rect.x = 350;
      rect.y = 85 * 5 + 2;
      if (frame % 20 >= 0 && frame % 20 < 10) {
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[1], &src_rect, screen, &rect);
      } else {
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
          rect.x = 660;
          SDL_BlitSurface(images[78], &src_rect, screen, &rect);
      }
      break;               


  }  // end switch
}

void wait_for_no_left_right_event()
{
  SDL_Event event;
  Uint8* keystate;
  
  do {
    SDL_PollEvent(&event);
    keystate = SDL_GetKeyState(NULL);

      if (event.type == SDL_JOYAXISMOTION ) {  
        // check if joystick 0 has moved
        if ( event.jaxis.which == 0 ) {
            //If the X axis changed
            if ( event.jaxis.axis == 0 ) {
                //If the X axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_left = 0;
                     joy_right = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                        joy_left = 1;
                        joy_right = 0;
                    } else {
                        joy_left = 0;
                        joy_right = 1;
                    }
                }
            }
            //If the Y axis changed
            else if ( event.jaxis.axis == 1 ) {
                //If the Y axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_up = 0;
                     joy_down = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                     joy_up = 1;
                     joy_down = 0;
                    } else {
                        joy_up = 0;
                        joy_down = 1;
                    }
                }
            }
        }   // if event.jaxis.which == 0   
      }  // event.type == SDL_JOYAXISMOTION

      if (last_joystick_action == TRUE &&
            joy_up == 0 && joy_down == 0 && joy_left == 0 && joy_right == 0 ) {        
            last_joystick_action = FALSE;
      }

  }  
  while ( keystate[SDLK_LEFT] == 1 || keystate[SDLK_RIGHT] == 1 || last_joystick_action == TRUE); 
}

// munchkin.c
