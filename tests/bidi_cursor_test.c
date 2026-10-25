#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct NVGcontext NVGcontext;
static int drawnCursor;
static char drawn[512];
static float tsvitchTextLtr(NVGcontext* ctx,float x,float y,const char* text,const char* end){(void)ctx;(void)x;(void)y;size_t n=end?(size_t)(end-text):strlen(text);memcpy(drawn,text,n);drawn[n]=0;return 1;}
static float tsvitchTextWithCursorLtr(NVGcontext* ctx,float x,float y,const char* text,const char* end,int cursor){drawnCursor=cursor;return tsvitchTextLtr(ctx,x,y,text,end);}
#include "../cmake/nanovg_bidi.c"
int main(void){
    nvgTextWithCursor(NULL,0,0,"שלום",NULL,4);assert(strcmp(drawn,"םולש")==0&&drawnCursor==0);
    nvgTextWithCursor(NULL,0,0,"שלום",NULL,0);assert(drawnCursor==4);
    nvgTextWithCursor(NULL,0,0,"שלום",NULL,2);assert(drawnCursor==2);
    nvgTextWithCursor(NULL,0,0,"Hello 123",NULL,3);assert(strcmp(drawn,"Hello 123")==0&&drawnCursor==3);
    nvgTextWithCursor(NULL,0,0,"שלום",NULL,-1);assert(drawnCursor==-1&&strcmp(drawn,"םולש")==0);
    nvgText(NULL,0,0,"English שלום 123",NULL);assert(strstr(drawn,"English")&&strstr(drawn,"םולש")&&strstr(drawn,"123"));
    return 0;
}
