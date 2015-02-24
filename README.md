Radio Weather Time

is simple application to display time,actual weather
and play your favorite internet radio using 
small lcd touch screen.

Setup:
 1. start mpd 

 2. add six internet radio to mpd playlist

 3. edit main.cpp

 
// ------------ Setup --------------------------

#define STATION1 "name1"
#define STATION2 "name2"
#define STATION3 "name3"
#define STATION4 "name4"
#define STATION5 "name5"
#define STATION6 "name6"
// display width support 320,480
#define WIDTH 480
// display height support 240,320
#define HEIGHT 320
#define RADIO_REFRESH_TIME 10
#define WEATHER_REFRESH_TIME 600

// exmaple 
char *WEATHER_URL_A = (char *)"http://api.wunderground.com/api/[add your key]/conditions/lang:SK/q/airport/LZIB.json";
char *WEATHER_URL_B = (char *)"http://api.wunderground.com/api/[add your key]/conditions/lang:SK/q/airport/LZSL.json";

// ---------------------------------------------


 4. install other needed libraries
    SDL 
    SDL_ttf
    SDL_draw 
    curl 
    mpdclient

 5. download some external libraries source
     cJSONFiles.zip http://sourceforge.net/projects/cjson/files/cJSONFiles.zip/download
     SDL_draw-1.2.13.tar.gz http://sdl-draw.sourceforge.net/
     compile and install SDL_draw
     copy SDL_draw.h cJSON.c cJSON.h to rwt main directory
 
 6. make 

 7. setup envoronment
   export SDL_FBDEV="/dev/fb1"
   export SDL_VIDEODRIVER="fbcon"
   export SDL_MOUSEDEV="/dev/input/touchscreen"
   export SDL_MOUSEDRV="TSLIB"
   export SDL_NOMOUSE=1

   export TSLIB_TSDEVICE="/dev/input/touchscreen"
   export TSLIB_CALIBFILE=/etc/pointercal
   export TSLIB_FBDEVICE=/dev/fb1

 8. run sudo ./rwt

