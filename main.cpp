#include <pthread.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <mpd/client.h>
#include <mpd/status.h>
#include <mpd/entity.h>
#include <mpd/search.h>
#include <mpd/tag.h>
#include <assert.h>

#include "SDL_draw.h"
#include "cJSON.h"

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
#define WEATHER_REFRESH_TIME 1200

// exmaple 
char *WEATHER_URL_A = (char *)"http://api.openweathermap.org/data/2.5/weather?id=3061186&units=metric&lang=sk&APPID=[your key]";
char *WEATHER_URL_B = (char *)"http://api.openweathermap.org/data/2.5/weather?id=3060322&units=metric&lang=sk&APPID=[your key]";

// ---------------------------------------------
const char* const WIND_DIR[]={"N","NNE","NE","ENE","E","ESE", "SE", "SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};

#if WIDTH==320
#define WEATHER_HISTORY 140
#else
#define WEATHER_HISTORY 220
#endif

#define CMD_OUT_MAX 100
#define WEATHER_OUT_MAX 100

#define ROUND_BOX_Y 53

#define DATETIME_Y 20
#define RADIO_X 10
#define RADIO_Y 56
#define WEATHER_X 15
#define RADIO_STATUS_Y 146
#define WEATHER_STATUS_X 5
#define WEATHER_STATUS_Y 163 
#define WEATHER_PRES 2

#define IP_ETH_X 20
#define IP_WLAN_X 230

#define FSIZE 20
#define DIFF 10
#define LDIFF 5

#define BUTTON_QUIT -1
#define BUTTON_STOP 10
#define BUTTON_PLAY 11
#define BUTTON_LOC  12
#define BUTTON_POWEROFF  13




struct MemoryStruct {
  char *memory;
  size_t size;
};


SDL_Color yellowColor = { 255, 255, 0 };
SDL_Color greenColor = { 0, 255, 0 };
SDL_Color whiteColor = { 255, 255, 255 };
SDL_Color brownColor = { 0xAA, 0x84, 0x39 };
SDL_Color greengrayColor = { 0x75, 0xAF, 0x96 };
SDL_Color darkGrayColor = { 0x60, 0x60, 0x60 };
SDL_Color redgrayColor = { 0xD8, 0xA7, 0xC5 };
SDL_Color cyanColor = { 0x0A, 0x91, 0xD9 };
SDL_Color redColor = {255, 0 , 0 };
SDL_Color blackColor = { 0, 0, 0 };
Uint32 lineColor=0xffffff;

SDL_Surface* screen = NULL;
int rotateScreen=0;

TTF_Font *font_control = NULL;
TTF_Font *font_text_date = NULL;
TTF_Font *font_text_radios = NULL;
TTF_Font *font_text_status = NULL;
TTF_Font *font_text_weather = NULL;
TTF_Font *font_text_ip = NULL;
uint lastRadioAlarmTime=0;
uint lastWeatherAlarmTime=0;

struct ifreq ifr_eth;
struct ifreq ifr_wlan;
int int_connection=0;

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

int quit=0;
uint radioPlay=0;
char cmd_out[CMD_OUT_MAX];  

int button=0;
int lastButton=0;

struct mpd_connection *mpc;


/* weather variables */
typedef struct Weather {
 char station_id[20];
 ulong observation_time;
 char weather_text[50];
 char wind_dir[5];
 double temp_c;
 double wind_kph;
 int wind_degrees;
 int pressure;
 int visibility;
 int humidity;
} Weather;

Weather weather_a;
Weather weather_b;
Weather *weather;

int temp_a[WEATHER_HISTORY];
int temp_b[WEATHER_HISTORY];
int pres_a[WEATHER_HISTORY];
int pres_b[WEATHER_HISTORY];
int weather_li=0;

int *temp;
int *pres;

int ip_wlan_x;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  return realsize;
}

void get_ifaddress(char *intName,ifreq *ifr) {
 int fd; 
 fd = socket(AF_INET, SOCK_DGRAM, 0);
 ifr->ifr_addr.sa_family = AF_INET;
 strncpy(ifr->ifr_name, intName, IFNAMSIZ-1);
 ioctl(fd, SIOCGIFADDR, ifr);
 close(fd);
 //printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr));
}

void get_int_ip() {
 int_connection=0;
 bzero(&ifr_eth,sizeof(ifr_eth)); 
 get_ifaddress((char *)"eth0",&ifr_eth);
 bzero(&ifr_wlan,sizeof(ifr_wlan)); 
 get_ifaddress((char *)"wlan0",&ifr_wlan);
 if (
   ((struct sockaddr_in *)&ifr_eth.ifr_addr)->sin_addr.s_addr!=0 ||
   ((struct sockaddr_in *)&ifr_wlan.ifr_addr)->sin_addr.s_addr!=0
  ) {
  int_connection=1;
 }
}

int get_url(char *url,Weather *lweather) {
  CURL *curl_handle;
  CURLcode res;
  struct MemoryStruct chunk;
  chunk.memory = (char *)malloc(1);
  chunk.size = 0;
  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL,url);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  res = curl_easy_perform(curl_handle);
  int out=0;
  
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
  } else {
    cJSON *json = cJSON_Parse(chunk.memory); 
    if (!json) {
     printf("Error before: [%s]\n",cJSON_GetErrorPtr());
     out=1;
    } else {
     cJSON *name=cJSON_GetObjectItem(json,"name"); 
     if (cJSON_IsString(name) && name->valuestring!=NULL) {
      bzero(lweather,sizeof(Weather));
      strcpy(lweather->station_id,name->valuestring);
      //printf("station_id:%s\n",lweather->station_id);
      
      cJSON *dt=cJSON_GetObjectItem(json,"dt"); 
      if (cJSON_IsNumber(dt)) {
       lweather->observation_time=dt->valueint;
       //printf("dt:%lu\n",lweather->observation_time);
      }
      
      cJSON *visibility=cJSON_GetObjectItem(json,"visibility"); 
      if (cJSON_IsNumber(visibility)) {
       lweather->visibility=visibility->valueint;
       //printf("visibility:%d\n",lweather->visibility);
      }
      
      cJSON *weathers=cJSON_GetObjectItem(json,"weather"); 
      if (weathers) {
       cJSON *item=NULL;
       cJSON_ArrayForEach(item,weathers) {
        cJSON *description=cJSON_GetObjectItem(item,"description");
        if (cJSON_IsString(description) && description->valuestring!=NULL) {
         strcpy(lweather->weather_text,description->valuestring);
         //printf("weather_text:%s\n",lweather->weather_text);
        }
       }
      }
      
      cJSON *main=cJSON_GetObjectItem(json,"main"); 
      if  (main) {
       cJSON *temp=cJSON_GetObjectItem(main,"temp");
       if (cJSON_IsNumber(temp)) {
        lweather->temp_c=temp->valuedouble;
        //printf("temp:%.1f\n",lweather->temp_c);
       }
       cJSON *pressure=cJSON_GetObjectItem(main,"pressure");
       if (cJSON_IsNumber(pressure)) {
        lweather->pressure=pressure->valueint;
        //printf("pressure:%d\n",lweather->pressure);
       }
       cJSON *humidity=cJSON_GetObjectItem(main,"humidity");
       if (cJSON_IsNumber(humidity)) {
        lweather->humidity=humidity->valueint;
        //printf("humidity:%d\n",lweather->humidity);
       }
      }
      
      cJSON *wind=cJSON_GetObjectItem(json,"wind");
      if (wind) {
       cJSON *speed=cJSON_GetObjectItem(wind,"speed");
       if (cJSON_IsNumber(speed)) {
        lweather->wind_kph=speed->valuedouble;
        //printf("wind_kph:%.1f\n",lweather->wind_kph);
       }
       cJSON *deg=cJSON_GetObjectItem(wind,"deg");
       if (cJSON_IsNumber(deg)) {
        lweather->wind_degrees=deg->valueint;
        //printf("wind degrees:%d\n",lweather->wind_degrees);                
        int val=int((lweather->wind_degrees/22.5)+.5);
        strcpy(lweather->wind_dir,WIND_DIR[(val % 16)]);
        //printf("wind_dir:%s\n",lweather->wind_dir);        
       }
      }    
     } else {
      out=1;
     }
     cJSON_Delete(json);
    }      
  }
  curl_easy_cleanup(curl_handle);
  if(chunk.memory)
    free(chunk.memory);
  curl_global_cleanup();
  return out;
}

void update_weather() {
 if (int_connection) {
  int out=0;
  out+=get_url(WEATHER_URL_A,&weather_a); 
  out+=get_url(WEATHER_URL_B,&weather_b);   
  if (out==0) {
   if (weather_li>=WEATHER_HISTORY) {
    for (int i=0;i<WEATHER_HISTORY-1;i++) {
     temp_a[i]=temp_a[i+1];
     temp_b[i]=temp_b[i+1];
     pres_a[i]=pres_a[i+1];
     pres_b[i]=pres_b[i+1];
    }  
    weather_li--;
   }
   temp_a[weather_li]=weather_a.temp_c;
   pres_a[weather_li]=weather_a.pressure;  
   temp_b[weather_li]=weather_b.temp_c;
   pres_b[weather_li]=weather_b.pressure;
   weather_li++;
  }
 } 
}

int getx(int x) {
 if (rotateScreen) {
  return (WIDTH-x);
 }
 return (x);
}

int gety(int y) {
 if (rotateScreen) {
  return (HEIGHT-y);
 }
 return (y);
}


SDL_Rect *Rect(int XPos, int YPos, int Width, int Height) {
 SDL_Rect *Rect = new SDL_Rect;
 Rect->h = Height;
 Rect->w = Width;
 Rect->x = XPos;
 Rect->y = YPos;     
 return Rect;
}

SDL_Rect *RectXY(int XPos, int YPos, int Width, int Height) {
 return (Rect(getx(XPos),gety(YPos),Width,Height));
}

int trimX(SDL_Surface *sf) {
 int i,j;
 int s=0;
 for (j=0;j<sf->pitch;j++) {
  for (i=0;i<sf->h;i++) {
   if (((Uint8 *)sf->pixels)[i*sf->pitch+j]!=0) {
    return s;
   }   
  }
  s++; 
 }
 return 0;
}

void apply_surface( int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip = NULL ) {
 SDL_Rect offset;
 offset.x = x;
 offset.y = y;
 if (rotateScreen) {
  int tx=trimX(source);
  SDL_Rect rect;
  rect.x=tx;
  rect.y=0;
  rect.w=source->w;
  rect.h=source->h;
  clip=Rect(source->pitch-source->w,0,source->w,source->h);
  SDL_BlitSurface( source, &rect, destination, &offset );    
 } else {
  SDL_BlitSurface( source, clip, destination, &offset );    
 }
 
}


void apply_surfaceXY( int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip = NULL ) { 
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x)-source->w;
  ny=gety(y)-source->h;
 } else {
  nx=x;ny=y;
 }  
 apply_surface(nx,ny,source,destination,clip);
}



int in_rectXY(int xp,int yp,int x,int y,int w,int h) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x)-w+1;
  ny=gety(y)-h+1;
 } else {
  nx=x;ny=y;  
 } 
 return (nx<=xp && nx+w>=xp && ny<=yp && ny+h>=yp); 
}

/*
void fill_rect(SDL_Surface *sf,int x,int y,int w,int h,Uint32 color) {
 SDL_Rect *rect = Rect(x,y,w,h);
 SDL_FillRect(sf,rect,color);
 delete rect;
}
*/

void fill_rectXY(SDL_Surface *sf,int x,int y,int w,int h,Uint32 color) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x)-w+1;
  ny=gety(y)-h+1;
 } else {
  nx=x;ny=y;
 }
 SDL_Rect rect;
 rect.x=nx;rect.y=ny;rect.w=w;rect.h=h;
 SDL_FillRect(sf,&rect,color);
}

/*
void clear_rect(SDL_Surface *sf,int x,int y,int w,int h) {
 //printf("clear rect x:%d y:%d w:%d h:%d\n",x,y,w,h);
 fill_rectXY(sf,x,y,w,h,0);
}
*/

void clear_rectXY(SDL_Surface *sf,int x,int y,int w,int h) {
 fill_rectXY(sf,x,y,w,h,0);
}


void SDL_UpdateRectXY(SDL_Surface *sf,int x,int y,int w, int h) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x)-w+1;
  ny=gety(y)-h+1;
 } else {
  nx=x;ny=y;
 }
 if (x==0 && y==0 && w==0 && h==0) {
  nx=0;ny=0;
 } 
 SDL_UpdateRect(sf,nx,ny,w,h);
}

/*
void rotate(SDL_Surface *sf) { 
  Uint8 *s;
  Uint8 *d;
  uint i;
  uint size=(sf->pitch*sf->h);
  Uint8 src;
  Uint8 dest;
  SDL_LockSurface(sf);
  for (i=0;i<size/2;i++) {
   s=(Uint8 *)(sf->pixels)+i;
   d=(Uint8 *)(sf->pixels)+size-i-1;
   src=*s;
   dest=*d;
   *s=dest;
   *d=src;
  } 
  SDL_UnlockSurface(sf);
}
*/

void rotate(SDL_Surface *sf) { 
  Uint8 *s;
  Uint8 *d;
  uint i;
  uint size=(sf->pitch*sf->h);
  Uint8 src;
  Uint8 dest;
//  Uint8 c=1;
  SDL_LockSurface(sf);
/*
  for (i=0;i<size;i++) {
   ((Uint8 *)(sf->pixels))[i]=c;
   if (c==1) c=0; else c=1;
  }
*/
  for (i=0;i<size/2;i++) {
   s=(Uint8 *)(sf->pixels)+i;
   d=(Uint8 *)(sf->pixels)+size-i-1;
   src=*s;
   dest=*d;
   *s=dest;
   *d=src;
  }
  SDL_UnlockSurface(sf);
}

void Draw_FillRoundXY(SDL_Surface *sf,Sint16 x,Sint16 y,Uint16 w,Uint16 h,Uint16 corner,Uint32 color) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x)-w;
  ny=gety(y)-h;
 } else {
  nx=x;
  ny=y;
 } 
 //printf("Draw_FillRoundXY nx:%d ny:%d w:%d h:%d\n",nx,ny,w,h);
 Draw_FillRound(sf,nx,ny,w,h,corner,color);
}


void Draw_HLineXY(SDL_Surface *sf,Sint16 x0,Sint16 y0, Sint16 x1,Uint32 color) {
 int nx0;
 int nx1;
 int ny0;
 if (rotateScreen) {
  nx0=getx(x0)-x0;
  nx1=getx(x1);
  ny0=gety(y0);  
 } else {
  nx0=x0;
  ny0=y0;
  nx1=x1;
 }
 Draw_HLine(sf,nx0,ny0,nx1,color);
}


void Draw_VLineXY(SDL_Surface *sf,Sint16 x0,Sint16 y0, Sint16 y1,Uint32 color) {
 int nx0;
 int ny0;
 int ny1;
 if (rotateScreen) {
  nx0=getx(x0);
  ny0=gety(y0);
  ny1=gety(y1);
 } else {
  nx0=x0;
  ny0=y0;
  ny1=y1;
 } 
 Draw_VLine(sf,nx0,ny0,ny1,color);
}

void Draw_RoundXY(SDL_Surface *sf,Sint16 x0,Sint16 y0, Uint16 w,Uint16 h,Uint16 corner, Uint32 color) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x0)-w;
  ny=gety(y0)-h;
 } else {
  nx=x0;
  ny=y0;
 }  
 Draw_Round(sf,nx,ny,w,h,corner,color);
}

void Draw_PixelXY(SDL_Surface *sf,Sint16 x, Sint16 y,Uint32 color) {
 int nx;
 int ny;
 if (rotateScreen) {
  nx=getx(x);
  ny=gety(y);
 } else {
  nx=x;
  ny=y;
 }   
 Draw_Pixel(sf,nx,ny,color);
}

void Draw_LineXY(SDL_Surface *sf,Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,Uint32 color) {
 int nx1;
 int ny1;
 int nx2;
 int ny2;
 
 if (rotateScreen) {
  nx1=getx(x1);
  ny1=gety(y1);
  nx2=getx(x2);
  ny2=gety(y2);  
 } else {
  nx1=x1;
  ny1=y1;
  nx2=x2;
  ny2=y2;
 }   
 Draw_Line(sf,nx1,ny1,nx2,ny2,color);
}

SDL_Surface* TTF_RenderText_SolidXY(TTF_Font *font,const char *buffer,SDL_Color color) {
 SDL_Surface *text = TTF_RenderText_Solid(font,buffer,color);  
 if (rotateScreen) rotate(text);
 return text;
}

SDL_Surface* TTF_RenderUTF8_SolidXY(TTF_Font *font,const char *buffer,SDL_Color color) { 
 SDL_Surface *text = TTF_RenderUTF8_Solid(font,buffer,color); 
 if (rotateScreen) rotate(text); 
 return text;
}

void run_command (char *cmd) {
 FILE *fp;
 char cmd_tmp[CMD_OUT_MAX];  
 bzero(cmd_tmp,sizeof(cmd_tmp));
 bzero(cmd_out,sizeof(cmd_out));
 fp = popen(cmd,"r");
 if (fp != NULL) {
  fgets(cmd_out,CMD_OUT_MAX, fp);
  int len = strlen(cmd_out);  
  if( cmd_out[len-1] == '\n' )
  cmd_out[len-1] = 0;
  char *song=strchr(cmd_out,':');
  if (song!=NULL) {
   strncpy(cmd_tmp,song+2,sizeof(cmd_tmp));
   strncpy(cmd_out,cmd_tmp,sizeof(cmd_out));
  }
  pclose(fp);
 }                               
}


void print_ip(SDL_Surface* sf,TTF_Font *font) {
 char buffer[30];
 SDL_Surface *text = NULL; 
 bzero(buffer,sizeof(buffer));
 snprintf(buffer,sizeof(buffer),"E:%s", inet_ntoa(((struct sockaddr_in *)&ifr_eth.ifr_addr)->sin_addr));
 text= TTF_RenderText_SolidXY(font,buffer, darkGrayColor ); 
 clear_rectXY(sf,IP_ETH_X,0,text->w,text->h);
 apply_surfaceXY(IP_ETH_X,0,text,sf);
 SDL_UpdateRectXY(sf,IP_ETH_X,0,IP_ETH_X+text->w,text->h); 
 SDL_FreeSurface( text );

 bzero(buffer,sizeof(buffer));
 snprintf(buffer,sizeof(buffer),"W:%s", inet_ntoa(((struct sockaddr_in *)&ifr_wlan.ifr_addr)->sin_addr));
 text= TTF_RenderText_SolidXY(font,buffer, darkGrayColor ); 
 clear_rectXY(sf,ip_wlan_x-text->w,0,text->w,text->h);
 apply_surfaceXY(ip_wlan_x-text->w,0,text,sf);
 SDL_UpdateRectXY(sf,IP_WLAN_X,0,ip_wlan_x,text->h);  
 SDL_FreeSurface( text );
}

void print_date_time(SDL_Surface* sf,TTF_Font *font) {
 char buffer[25]; 
 time_t timer;
 struct tm* tm_info;
 time(&timer);
 tm_info = localtime(&timer);
 strftime(buffer,sizeof(buffer), "%d.%m.%Y %H:%M:%S", tm_info);
 SDL_Surface *text = NULL;
 text=TTF_RenderText_SolidXY(font,buffer, yellowColor ); 
 clear_rectXY(sf,(WIDTH-text->w)/2,DATETIME_Y,text->w,text->h);
 apply_surfaceXY((WIDTH-text->w)/2,DATETIME_Y,text,sf);
 SDL_UpdateRectXY(sf,(WIDTH-text->w)/2,DATETIME_Y,(WIDTH-text->w)/2+text->w,DATETIME_Y+text->h); 
 SDL_FreeSurface( text );
}

void print_weather_status(SDL_Surface* sf,TTF_Font *font) {
 clear_rectXY(sf,WEATHER_STATUS_X,WEATHER_STATUS_Y,WIDTH-2*WEATHER_STATUS_X,32); 
 SDL_Surface *text = NULL; 
 char weather_out[WEATHER_OUT_MAX];  
 
 bzero(weather_out,sizeof(weather_out));
 char buffer[25];
 time_t timer=weather->observation_time;; 
 struct tm* tm_info;
 tm_info = localtime(&timer);
 strftime(buffer,sizeof(buffer), "%H:%M", tm_info);
 snprintf(weather_out,sizeof(weather_out),"%s",buffer);

 text = TTF_RenderText_SolidXY(font,weather_out,yellowColor);
 apply_surfaceXY(WEATHER_STATUS_X,WEATHER_STATUS_Y,text,sf); 
 int sp=4;
 int tw=text->w+sp; 
 SDL_FreeSurface( text );

 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%.1f%cC",weather->temp_c,176);
 text = TTF_RenderText_SolidXY(font,weather_out,redColor); 
 apply_surfaceXY(WEATHER_STATUS_X+tw,WEATHER_STATUS_Y,text,sf);
 tw+=text->w+sp;
 SDL_FreeSurface( text );
 
 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%.1f Km/h,%s",weather->wind_kph,weather->wind_dir);
 text = TTF_RenderUTF8_SolidXY(font,weather_out,brownColor); 
 apply_surfaceXY(WEATHER_STATUS_X+tw,WEATHER_STATUS_Y,text,sf);
 tw+=text->w+sp;
 SDL_FreeSurface( text );

 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%d hPa",weather->pressure);
 text = TTF_RenderText_SolidXY(font,weather_out,greengrayColor); 
 apply_surfaceXY(WEATHER_STATUS_X+tw,WEATHER_STATUS_Y,text,sf);
 tw+=text->w+sp;
 SDL_FreeSurface( text );

 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%d %%",weather->humidity);
 text = TTF_RenderText_SolidXY(font,weather_out,redgrayColor); 
 apply_surfaceXY(WEATHER_STATUS_X+tw,WEATHER_STATUS_Y,text,sf);
 tw+=text->w+sp;
 SDL_FreeSurface( text );

 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%s",weather->station_id);
 text = TTF_RenderText_SolidXY(font,weather_out,yellowColor); 
 apply_surfaceXY(WEATHER_STATUS_X,WEATHER_STATUS_Y+15,text,sf); 
 SDL_FreeSurface( text );

 bzero(weather_out,sizeof(weather_out));
 snprintf(weather_out,sizeof(weather_out),"%s",weather->weather_text);
 text = TTF_RenderUTF8_SolidXY(font,weather_out,cyanColor); 
 apply_surfaceXY((WIDTH-text->w)/2,WEATHER_STATUS_Y+15,text,sf); 
 SDL_FreeSurface( text );
 
 SDL_UpdateRectXY(sf,WEATHER_STATUS_X,WEATHER_STATUS_Y,WIDTH-2*WEATHER_STATUS_X,32);   
 
 // print temp,pressure graphs
 int min_temp=INT_MAX;
 int max_temp=INT_MIN;
 int min_pres=INT_MAX;
 int max_pres=INT_MIN;
 for (int i=0;i<weather_li;i++) {
  if (temp[i]<min_temp) min_temp=temp[i];
  if (temp[i]>max_temp) max_temp=temp[i];
  if (pres[i]<min_pres) min_pres=pres[i];
  if (pres[i]>max_pres) max_pres=pres[i];  
 } 

 int yscale=1;
 int scaleLimit=(HEIGHT-WEATHER_STATUS_Y-35)/2;
 int yshift=min_temp+(max_temp-min_temp)/2;;

 if(max_temp-min_temp<scaleLimit && max_temp-min_temp>0) {
  yscale=scaleLimit/(max_temp-min_temp);
 }
 clear_rectXY(sf,WEATHER_X,WEATHER_STATUS_Y+35,WIDTH/2-WEATHER_X,HEIGHT-WEATHER_STATUS_Y-36);   
 if (weather_li<=1) {
  Draw_PixelXY(sf,WEATHER_X,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-temp[0]+yshift,0xf000);
 } else {
  for (int i=0;i<weather_li-1;i++) {
   Draw_LineXY(sf,WEATHER_X+i,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-yscale*(temp[i]-yshift),WEATHER_X+i+1,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-yscale*(temp[i+1]-yshift),0xf000);
  } 
 }
 SDL_UpdateRectXY(sf,WEATHER_X,WEATHER_STATUS_Y+35,WIDTH/2-WEATHER_X,HEIGHT-WEATHER_STATUS_Y-36);   


 yscale=1;
 yshift=min_pres+(max_pres-min_pres)/2;
 if(max_pres-min_pres<scaleLimit && max_pres-min_pres>0) {
  yscale=scaleLimit/(max_pres-min_pres);  
 }
 clear_rectXY(sf,WEATHER_PRES+WIDTH/2,WEATHER_STATUS_Y+35,WIDTH/2-WEATHER_X,HEIGHT-WEATHER_STATUS_Y-36);   
 if (weather_li<=1) {
  Draw_PixelXY(sf,WEATHER_PRES+WIDTH/2,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-pres[0]+yshift,0x7572);
 } else {
  for (int i=0;i<weather_li-1;i++) {
   Draw_LineXY(sf,WEATHER_PRES+WIDTH/2+i,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-yscale*(pres[i]-yshift),WEATHER_PRES+WIDTH/2+i+1,HEIGHT-(HEIGHT-WEATHER_STATUS_Y-35)/2-yscale*(pres[i+1]-yshift),0x7572);
  } 
 }
 SDL_UpdateRectXY(sf,WEATHER_PRES+WIDTH/2,WEATHER_STATUS_Y+35,WIDTH/2-WEATHER_X,HEIGHT-WEATHER_STATUS_Y-36);   

}

static int handle_error(struct mpd_connection *c) {
 assert(mpd_connection_get_error(c) != MPD_ERROR_SUCCESS);
 fprintf(stderr, "%s\n", mpd_connection_get_error_message(c));
 return EXIT_FAILURE;
}

void print_radio_status(SDL_Surface* sf,TTF_Font *font) {
 struct mpd_status * status;
 struct mpd_song *song;  
 mpd_command_list_begin(mpc, true);
 mpd_send_status(mpc); 
 mpd_send_current_song(mpc);
 mpd_command_list_end(mpc);
 status = mpd_recv_status(mpc); 
 if (status == NULL) {
  printf("mpc status error!\n");  
  handle_error(mpc);  
 } else {
  if (mpd_status_get_state(status) == MPD_STATE_PLAY) {      
   char buffer[CMD_OUT_MAX];
   bzero(buffer,sizeof(buffer));
   mpd_response_next(mpc);    
   if ((song = mpd_recv_song(mpc)) != NULL) {
    const char *title = mpd_song_get_tag(song,MPD_TAG_TITLE,0);
    const char *name = mpd_song_get_tag(song,MPD_TAG_NAME,0);

    if (title!=NULL) {
     strncpy(buffer,title,sizeof(buffer));
    } else {
     if (name!=NULL) {
      strncpy(buffer,name,sizeof(buffer));
     }          
    }
    mpd_song_free(song);
   }   
   mpd_status_free(status);
   
   clear_rectXY(sf,RADIO_X,RADIO_STATUS_Y,WIDTH-2*RADIO_X,12); 
   SDL_Surface *text = NULL;
   text=TTF_RenderText_SolidXY(font,buffer,yellowColor);
   SDL_Rect *rect=Rect(0,0,WIDTH-2*RADIO_X,12);
   apply_surfaceXY(RADIO_X,RADIO_STATUS_Y,text,sf,rect);
   SDL_UpdateRectXY(sf,RADIO_X,RADIO_STATUS_Y,WIDTH-2*RADIO_X,12);   
   SDL_FreeSurface( text );
  }  
 } 
 mpd_response_finish(mpc);
}

void print_radios(SDL_Surface* sf,TTF_Font *font,int xm,int ym) {
 uint radioPlayOld=radioPlay;
 SDL_Color selectedFgColor=yellowColor;
 SDL_Color unSelectedFgColor=greenColor;
 SDL_Surface *text =NULL;
 
 Uint32 selectedBgColor=1234;
 Uint32 unSelectedBgColor=0;

 SDL_Color fgColor;
 Uint32 bgColor;
 
 
 clear_rectXY(sf,RADIO_X,RADIO_Y,300,0); 
 
 if (button==1) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=1;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }
 text = TTF_RenderText_SolidXY(font,STATION1,fgColor);  
 Draw_FillRoundXY(sf,RADIO_X,RADIO_Y,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X,RADIO_Y,text,sf);
 SDL_FreeSurface( text );

 if (button==2) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=2;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }  
 text = TTF_RenderText_SolidXY(font,STATION2, fgColor );  
 Draw_FillRoundXY(sf,RADIO_X+WIDTH/2,RADIO_Y,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X+WIDTH/2,RADIO_Y,text,sf);
 SDL_FreeSurface( text );


 if (button==3) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=3;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }
 text = TTF_RenderText_SolidXY(font,STATION3, fgColor );  
 Draw_FillRoundXY(sf,RADIO_X,RADIO_Y+FSIZE+DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X,RADIO_Y+FSIZE+DIFF,text,sf);
 SDL_FreeSurface( text );
 
 if (button==4) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=4;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }
 text = TTF_RenderText_SolidXY(font,STATION4, fgColor );  
 Draw_FillRoundXY(sf,RADIO_X+WIDTH/2,RADIO_Y+FSIZE+DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X+WIDTH/2,RADIO_Y+FSIZE+DIFF,text,sf);
 SDL_FreeSurface( text );

 if (button==5) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=5;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }
 text = TTF_RenderText_SolidXY(font,STATION5, fgColor );  
 Draw_FillRoundXY(sf,RADIO_X,RADIO_Y+2*FSIZE+2*DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X,RADIO_Y+2*FSIZE+2*DIFF,text,sf);
 SDL_FreeSurface( text );

 if (button==6) {
  fgColor=selectedFgColor;
  bgColor=selectedBgColor;
  radioPlay=6;
 }  else {
  fgColor=unSelectedFgColor;
  bgColor=unSelectedBgColor;
 }
 text = TTF_RenderText_SolidXY(font,STATION6, fgColor );
 Draw_FillRoundXY(sf,RADIO_X+WIDTH/2,RADIO_Y+2*FSIZE+2*DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2,10,bgColor);
 apply_surfaceXY(LDIFF+RADIO_X+WIDTH/2,RADIO_Y+2*FSIZE+2*DIFF,text,sf);
 SDL_FreeSurface( text );

 SDL_UpdateRectXY(sf,RADIO_X,RADIO_Y,WIDTH-RADIO_X,HEIGHT-RADIO_Y);  
 
 if (radioPlayOld!=radioPlay) {
  radioPlayOld=radioPlay;
  mpd_run_play_pos(mpc,radioPlay-1);
  print_radio_status(sf,font_text_status);
 } 
}


int get_button(int mx,int my) {
 int b=0;
 if (in_rectXY(mx,my,RADIO_X,RADIO_Y,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
  b=1;
 } else {
  if (in_rectXY(mx,my,RADIO_X+WIDTH/2,RADIO_Y,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
   b=2;
  } else {
   if (in_rectXY(mx,my,RADIO_X,RADIO_Y+FSIZE+DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
    b=3;
   } else {
    if (in_rectXY(mx,my,RADIO_X+WIDTH/2,RADIO_Y+FSIZE+DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
     b=4;
    } else {
     if (in_rectXY(mx,my,RADIO_X,RADIO_Y+2*FSIZE+2*DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
      b=5;
     } else {
      if (in_rectXY(mx,my,RADIO_X+WIDTH/2,RADIO_Y+2*FSIZE+2*DIFF,WIDTH/2-2*RADIO_X,FSIZE+DIFF/2)) {
       b=6;
      } else {
       if (in_rectXY(mx,my,WIDTH-40,0,40,24)) {
        b=BUTTON_POWEROFF;
        quit=1;
       } else {
        if (in_rectXY(mx,my,0,0,40,24)) {
         b=BUTTON_STOP;
        } else {
         if (in_rectXY(mx,my,WEATHER_STATUS_X,WEATHER_STATUS_Y,50,30)) {
          b=BUTTON_LOC;          
         } else {
          if (in_rectXY(mx,my,WIDTH-40,HEIGHT-24,40,24)) {
           b=BUTTON_QUIT;
           quit=1;
          }
         }
        }
       }
      }
     }
    }
   }
  } 
 } 
 return (b);
}

void catch_alarm (int sig) {
 if (!pthread_mutex_trylock (&mutex)) {
  uint alarmTime=time(NULL);
  print_date_time(screen,font_text_date);
  if (alarmTime-lastRadioAlarmTime>RADIO_REFRESH_TIME) {
   print_radio_status(screen,font_text_status);
   lastRadioAlarmTime=alarmTime;
  }
  if (alarmTime-lastWeatherAlarmTime>WEATHER_REFRESH_TIME) {
   get_int_ip();  
   print_ip(screen,font_text_ip);
   update_weather();
   print_weather_status(screen,font_text_weather);
   lastWeatherAlarmTime=alarmTime;
  }
  signal (sig, catch_alarm); 
  pthread_mutex_unlock (&mutex);
 }
 alarm(1);
 
}


void close() {
 mpd_run_stop (mpc);
 TTF_CloseFont( font_control );
 TTF_CloseFont( font_text_date );
 TTF_CloseFont( font_text_radios );
 TTF_CloseFont( font_text_status );
 TTF_CloseFont( font_text_weather );
 TTF_CloseFont( font_text_ip );
 TTF_Quit();
 SDL_Quit(); 
}



void gen_tp() {
 for (int i=0;i<WEATHER_HISTORY;i++) {
  temp_a[i]=i%20-10;
  pres_a[i]=i%20-10+1020;
 }
 weather_li=WEATHER_HISTORY;
}


int main(int argc, char **argv){

 printf("RADIO (C) MM 2014\n");

 mpc = mpd_connection_new(NULL, 0, 30000);
 if (mpd_connection_get_error(mpc) != MPD_ERROR_SUCCESS) {
  handle_error(mpc);
  mpd_connection_free(mpc);
  printf("mpd not running!\n");
  return (-1);
 }

 weather=&weather_a;
 temp=temp_a;
 pres=pres_a;
 bzero(temp_a,sizeof(temp_a));
 bzero(temp_b,sizeof(temp_b));
 bzero(pres_a,sizeof(pres_a));
 bzero(pres_b,sizeof(pres_b));
   
   
// gen_tp();  
 get_int_ip();  

 
 signal (SIGALRM, catch_alarm);
 
 if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL_Init Error: %s\n",SDL_GetError());
    return 1;
 } else {
   printf("SDL initialized successfully\n");  	  	  
   screen = SDL_SetVideoMode(WIDTH,HEIGHT, 16, SDL_HWSURFACE|SDL_FULLSCREEN );
   if (screen!= NULL) {	    
     SDL_ShowCursor(SDL_DISABLE);
     if (TTF_Init()==0) {
      SDL_Surface *text  = NULL;
      font_control = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 16 );
      font_text_date = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 28 );
      font_text_radios = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 22 );
      font_text_status = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 10 );
      font_text_weather = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 14 );
      font_text_ip = TTF_OpenFont( "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",8 );
      text = TTF_RenderText_SolidXY( font_control, "#", whiteColor );
      apply_surfaceXY(0, 0, text, screen );
      
      text = TTF_RenderText_SolidXY( font_control, "RWT", redColor );  	    
      apply_surfaceXY( (WIDTH-text->w)/2, 0, text, screen );      
      SDL_FreeSurface( text );

      text = TTF_RenderText_SolidXY( font_control, "X", whiteColor );
      apply_surfaceXY( WIDTH-text->w, 0, text, screen );      
      ip_wlan_x=WIDTH-text->w-5;
      SDL_FreeSurface( text );
      Draw_RoundXY(screen,0,ROUND_BOX_Y,WIDTH,HEIGHT-ROUND_BOX_Y,20,0xffffff);
      print_radios(screen,font_text_radios,0,0);
      print_ip(screen,font_text_ip);

      Draw_HLineXY(screen,0,RADIO_Y+FSIZE+DIFF-3,WIDTH,lineColor);
      Draw_HLineXY(screen,0,RADIO_Y+2*FSIZE+2*DIFF-3,WIDTH,lineColor);
      Draw_HLineXY(screen,0,RADIO_Y+3*FSIZE+3*DIFF-3,WIDTH,lineColor);
      Draw_HLineXY(screen,0,RADIO_Y+3*FSIZE+3*DIFF+14,WIDTH,lineColor);
      Draw_HLineXY(screen,0,RADIO_Y+3*FSIZE+3*DIFF+50,WIDTH,lineColor);
      Draw_VLineXY(screen,WIDTH/2,RADIO_Y-1,RADIO_Y+3*FSIZE+3*DIFF-3,lineColor);
      Draw_VLineXY(screen,WIDTH/2,RADIO_Y+3*FSIZE+3*DIFF+51,HEIGHT-1,lineColor);
      
      SDL_UpdateRectXY(screen,0,0,0,0);
      
      SDL_Event event;

      Sint16 lx=0,ly=0,nx=0,ny=0;  	      	    
      alarm(1);
      while( SDL_WaitEvent(&event) && !quit) {
        pthread_mutex_lock (&mutex);
        switch(event.type) {
         case SDL_QUIT: 
          quit=1;
          break;
         case SDL_MOUSEBUTTONDOWN: 
           button=get_button(event.motion.x,event.motion.y);
           printf("get button %d\n",button);
           if (lastButton!=button) {
            if (button==BUTTON_STOP) {
              radioPlay=0;
              mpd_run_stop (mpc);
              clear_rectXY(screen,RADIO_X,RADIO_STATUS_Y,WIDTH-2*RADIO_X,12); 
              SDL_UpdateRectXY(screen,RADIO_X,RADIO_STATUS_Y,WIDTH-2*RADIO_X,12);   
            }
            if (button==BUTTON_POWEROFF) {
              radioPlay=0;
              run_command((char*) "poweroff");
            }
            if ((button>=1 && button<=6) || button==BUTTON_STOP) {
             print_radios(screen,font_text_radios,event.motion.x,event.motion.y);
            } 
            if (button==BUTTON_LOC) {
              Weather *w;
              if (weather==&weather_a) {temp=temp_b;pres=pres_b;w=&weather_b;}
              if (weather==&weather_b) {temp=temp_a;pres=pres_a;w=&weather_a;}             
              weather=w;
              print_weather_status(screen,font_text_weather);
              button=0;
            }
            lastButton=button;
           }
           break;
         case SDL_MOUSEMOTION:  
           nx=event.motion.x;
           ny=event.motion.y;
           if (lx && ly) {
            //Draw_Line(screen,lx,ly,nx,ny,0xffffff); 		  
            //SDL_UpdateRectXY(screen,0,0,0,0); 
           }
           lx=nx;ly=ny;
           break;   	          
         default: 
           //printf("Unhandled Event! %d\n",event.type);  
           break;           
        }        
        pthread_mutex_unlock (&mutex);
      }  	     
      close();				  	      	    
     } else {
       printf("TTF_Init Error: %s\n",SDL_GetError());
       SDL_Quit();             
       return 1;
     } 
   } else {
    printf("SDL_SetVideoMode Error: %s\n",SDL_GetError());
    SDL_Quit();
    return 1;
   }
 }        
 mpd_connection_free(mpc);
 printf("end\n");        
 return 0;	          
}
