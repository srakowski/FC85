// fc85 - A Fantasy Console developed for #FCDEV_JAM 2017
// Created by Shawn Rakowski

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdbool.h>
#include "rom.h"

#define BGCOL             0xFF5C4530
#define FGCOL             0xFF99C278

#define NUM_SPRITES       256
#define MAX_FILE_SIZE     16384
#define DISK_SIZE         163840

#define FC_NAME           "FC-85"
#define DISPLAY_WIDTH     96
#define DISPLAY_HEIGHT    64
#define CHAR_CELL_WIDTH   6
#define CHAR_CELL_HEIGHT  8
#define CHAR_CELL_COLS    (DISPLAY_WIDTH/CHAR_CELL_WIDTH)
#define CHAR_CELL_ROWS    (DISPLAY_HEIGHT/CHAR_CELL_HEIGHT)

#define CH_FLAG_NONE    0x00
#define CH_FLAG_INVERT  0x01

#define STR_FLAG_NONE   CH_FLAG_NONE
#define STR_FLAG_INVERT CH_FLAG_INVERT

#define CLR_FLAG_FBUF   0x01
#define CLR_FLAG_CBUF   0x02
#define CLR_FLAG_ALL    0xFF

#define BTN_ESCP        0x0400
#define BTN_RETN        0x0200
#define BTN_BKSP        0x0100
#define BTN_UP          0x0080
#define BTN_DOWN        0x0040
#define BTN_LEFT        0x0020
#define BTN_RIGHT       0x0010
#define BTN_START       0x0008
#define BTN_SELECT      0x0004
#define BTN_A           0x0002
#define BTN_B           0x0001

#define arraylen(x) (sizeof((x))/sizeof((x)[0]))

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;

typedef union {
  int value;
  struct {
    byte r;
    byte g;
    byte b;
    byte a;
  } rgba;
} color;

typedef struct {
  byte name[16];
  byte sprt[NUM_SPRITES][8];
  byte font[256][8];
} Assets;

typedef struct {
  Assets assets;
  byte code[MAX_FILE_SIZE - sizeof(Assets)];
} Game;

typedef byte GameData[sizeof(Game)];

typedef struct {
  char name[16];
  void (*onPick)(void *menuItem, void *sys);
} MenuItem;

typedef struct {
  byte initialized;
  byte active;
  byte count;
  byte head;
  MenuItem items[32];
} Menu;

typedef struct {
  byte initialized;
  byte startRow;
  byte startCol;
  byte maxlen;
  byte count;
  byte buffer[256];
  byte cursorOn;
  float cursorTimer;
  void (*onReturn)(void *, void *);
} TextInputBuffer;

typedef struct {
  byte name[16];
  byte data[MAX_FILE_SIZE];
  void (*exe)(void *cmp, void *sys);
} Component;

typedef struct {
  struct {
    Component *cmp;
    void (*onDestroy)(Component *);
  } cmpLst[8];
  byte cmpCnt;
  byte actvCmp;
  byte visCmpHead;
} Module;

typedef struct {
  byte fbuf[DISPLAY_WIDTH/8][DISPLAY_HEIGHT];
  struct {
    byte c;
    byte flags;
  } cbuf[CHAR_CELL_ROWS][CHAR_CELL_COLS];
  byte font[256][8];
  color bgcol;
  color fgcol;
} Display;

typedef struct {
  Display dsp;
  byte count;
  struct {
    Module *mdl;
    void (*onPop)(Module *);
  } moduleStack[8]; // the module stack
  byte pCount;
  struct {
    Module *mdl;
    void (*onPop)(Module *);
  } pModuleStack[8]; // modules that have been popped and need to be cleaned up
  byte shutdown;
  word btns; // btn map
  float delta; // update time delta
  byte tbuf[256]; // text input buffer
  byte fswap[MAX_FILE_SIZE]; // file swap space for passing files between components
} System;

typedef struct Machine {
  System sys;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  byte disk[DISK_SIZE];
} Machine;

/* ------------------------------------------------------------------------- */
// DISPLAY
/* ------------------------------------------------------------------------- */

static void display_PT(Display *dsp, byte x, byte y, byte on) {
  assert(dsp);
  byte px = x / 8;
  byte bx = x % 8;
  if (on)
    dsp->fbuf[px][y] |= (0x80 >> bx);
  else
    dsp->fbuf[px][y] &= ((0x80 >> bx) ^ 0xFF);
}

static void display_CH(Display *dsp, byte row, byte col, byte c, byte flags) {
  assert(dsp);
  dsp->cbuf[row][col].c = c;
  dsp->cbuf[row][col].flags = flags;
}

static void display_STR(Display *dsp, byte row, byte col, const char *str, byte flags) {
  assert(dsp);
  for (byte i = 0, c = col; i < strlen(str); i++, c++)
    display_CH(dsp, row, c, str[i], flags);
}

static void display_RCBUF(Display *dsp) {
  assert(dsp);
  for (int r = 0; r < CHAR_CELL_ROWS; r++)
  for (int c = 0; c < CHAR_CELL_COLS; c++)
  {
    byte *font_char = dsp->font[dsp->cbuf[r][c].c];
    byte dy = r * CHAR_CELL_HEIGHT;
    byte dx = c * CHAR_CELL_WIDTH;
    for (int cy = 0; cy < CHAR_CELL_HEIGHT; cy++)
    for (int cx = 0; cx < CHAR_CELL_WIDTH; cx++)
    {
      byte on = (font_char[cy] & (0x80 >> cx));
      on = (dsp->cbuf[r][c].flags & CH_FLAG_INVERT) ? !on : on;
      display_PT(dsp, dx + cx, dy + cy, on);
    }
  }
}

static void display_CLR(Display *dsp, byte flags) {
  if (flags & CLR_FLAG_FBUF)
    memset(dsp->fbuf, 0, sizeof(dsp->fbuf));
  if (flags & CLR_FLAG_CBUF)
    memset(dsp->cbuf, 0, sizeof(dsp->cbuf));
}

/* ------------------------------------------------------------------------- */
// MENU
/* ------------------------------------------------------------------------- */

void menu_HandleInput(Menu *menu, System *sys) {
  if (sys->btns & BTN_A)
  {
    menu->items[menu->active].onPick(&menu->items[menu->active], sys);
  }
  if (sys->btns & BTN_UP)
  {
    menu->active--;
    menu->active = (sbyte)menu->active < 0 ? 0 : menu->active;
    if (menu->active < menu->head)
      menu->head--;
  }
  if (sys->btns & BTN_DOWN)
  {
    menu->active++;
    menu->active = menu->active >= menu->count
      ? menu->count - 1
      : menu->active;
    if (menu->active - menu->head >= 3)
      menu->head++;
  }
}

void menu_Draw(Menu *menu, System *sys) {
  for (int m = 0; m < menu->count; m++) {
    char name[17] = {'\0'};
    sprintf(name, "%d:", (m + 1) < 10 ? (m + 1) : 0);
    display_STR(&sys->dsp, m+1, 0, name, m == menu->active ? STR_FLAG_INVERT : STR_FLAG_NONE);
    strncpy(name, menu->items[m].name, sizeof(name) - 3);
    display_STR(&sys->dsp, m+1, 2, name, STR_FLAG_NONE);
  }
}

/* ------------------------------------------------------------------------- */
// TEXT INPUT BUFFER
/* ------------------------------------------------------------------------- */

void textInputBuffer_HandleInput(TextInputBuffer *inp, System *sys) {
  if (sys->btns & BTN_RETN) {
    inp->onReturn(inp->buffer, sys);
    return;
  } else if (sys->btns & BTN_BKSP && inp->count > 0) {
    inp->buffer[inp->count] = 0;
    inp->count--;
  } else {
    // todo: maybe memcpy this instead
    for (int c = 0; c < strlen(sys->tbuf) && 
          inp->count < inp->maxlen && 
          inp->count < sizeof(inp->buffer); c++) {
      inp->buffer[inp->count] = sys->tbuf[c];
      inp->count++;
    }
  }
  inp->cursorTimer += sys->delta;
  if (inp->cursorTimer > 0.5f) {
    inp->cursorOn = !inp->cursorOn;
    inp->cursorTimer = 0;
  }
}

void textInputBuffer_Draw(TextInputBuffer *inp, System *sys) {
  byte row = inp->startRow;
  byte col = inp->startCol;
  for (int c = 0; c < inp->count; c++) {
    display_CH(&sys->dsp, row, col, inp->buffer[c], CH_FLAG_NONE);
    col++;
    if (col >= CHAR_CELL_COLS) {
      col = 0;
      row++;
      // todo: rows too many? add shift up func to char cell display
    }
  }
  // CURSOR
  display_CH(&sys->dsp, row, col, 219, inp->cursorOn ? CH_FLAG_NONE : CH_FLAG_INVERT);
}

/* ------------------------------------------------------------------------- */
// Component
/* ------------------------------------------------------------------------- */

static Component *component_Create(const char *name, void (*exe)(void *, void *)) {
  Component *self = (Component *)calloc(1, sizeof(Component));
  memcpy(self->name, name, min(sizeof(self->name) - 1, strlen(name)));
  self->exe = exe;
  return self;
}

static void component_Destroy(Component *self) {
  free(self);
}

/* ------------------------------------------------------------------------- */
// MODULE
/* ------------------------------------------------------------------------- */

static Module *module_Create() {
  Module *self = (Module *)calloc(1, sizeof(Module));
  self->cmpCnt = 0;
  return self;
}

static void module_AddComponent(Module *self, Component *cmp, void (*onDestroy)(Component *)) {  
  assert(self->cmpCnt < arraylen(self->cmpLst));
  self->cmpLst[self->cmpCnt].onDestroy = onDestroy;
  self->cmpLst[self->cmpCnt].cmp = cmp;
  self->cmpCnt++;
}

static void module_Destroy(Module *self) {
  for (int c = 0; c < self->cmpCnt; c++)
    if (self->cmpLst[c].onDestroy)
      self->cmpLst[c].onDestroy(self->cmpLst[c].cmp);
  free(self);
}

/* ------------------------------------------------------------------------- */
// SYSTEM
/* ------------------------------------------------------------------------- */

static void system_PushModule(System *sys, Module *module, void (*onPop)(Module *)) {
  sys->moduleStack[sys->count].mdl = module;
  sys->moduleStack[sys->count].onPop = onPop;
  sys->count++;
}

static void system_PopModule(System *sys) {
  memcpy(&sys->pModuleStack[sys->pCount],
    &sys->moduleStack[sys->count],
    sizeof(sys->pModuleStack[sys->pCount])
  );
  sys->pCount++;
  sys->count--;
  if (sys->count < 0)
    sys->shutdown = true;
}

static void system_Tick(System *sys) {
  if (sys->shutdown)
    return;

  display_CLR(&sys->dsp, CLR_FLAG_ALL);
  
  if (sys->btns & BTN_ESCP) {
    system_PopModule(sys);
  }

  Module* m = sys->moduleStack[sys->count - 1].mdl;

  if (m->cmpCnt > 1) {
    if (sys->btns & BTN_RIGHT)
    {
      m->actvCmp++;
      m->actvCmp = m->actvCmp >= m->cmpCnt 
        ? m->cmpCnt - 1
        : m->actvCmp;
      if (m->actvCmp - m->visCmpHead >= 3)
        m->visCmpHead++;
    }

    if (sys->btns & BTN_LEFT)
    {
      m->actvCmp--;
      m->actvCmp = (sbyte)m->actvCmp < 0
        ? 0
        : m->actvCmp;
      if (m->actvCmp < m->visCmpHead)
        m->visCmpHead--;
    }

    for (int c = m->visCmpHead, i = 0; c < m->cmpCnt && i < 3; c++, i++) {
      display_STR(&sys->dsp, 0, (i * 5) + (m->visCmpHead > 0 ? 1 : 0), m->cmpLst[c].cmp->name, 
        c == m->actvCmp ? STR_FLAG_INVERT : STR_FLAG_NONE);
    }

    if (m->cmpCnt - m->visCmpHead > 3) {
      display_CH(&sys->dsp, 0, CHAR_CELL_COLS - 1, 240, CH_FLAG_NONE);
    } 
    if (m->visCmpHead > 0) {
      display_CH(&sys->dsp, 0, 0, 240, CH_FLAG_NONE);
    }
  } else {
    display_STR(&sys->dsp, 0, 0, m->cmpLst[0].cmp->name, STR_FLAG_NONE);
  }

  display_RCBUF(&sys->dsp);

  m->cmpLst[m->actvCmp].cmp->exe(
    m->cmpLst[m->actvCmp].cmp,
    sys
  );

  while (sys->pCount > 0) {
    sys->pCount--;
    if (sys->pModuleStack[sys->pCount].onPop) {
      sys->pModuleStack[sys->pCount].onPop(sys->pModuleStack[sys->pCount].mdl);
    }
  }
}

/* ------------------------------------------------------------------------- */
// MACHINE
/* ------------------------------------------------------------------------- */

static Machine *machine_Create() {
  Machine *m = (Machine *)calloc(1, sizeof(Machine));
  if (m == NULL)
    return NULL;

  memset(m, 0, sizeof(Machine));

  memcpy(m->sys.dsp.font, font_rom, sizeof(m->sys.dsp.font));
  m->sys.dsp.bgcol.value = BGCOL;
  m->sys.dsp.fgcol.value = FGCOL;


  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    exit(EXIT_FAILURE);


  SDL_ShowCursor(SDL_DISABLE);

  m->window = SDL_CreateWindow(FC_NAME,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    DISPLAY_WIDTH * 10, DISPLAY_HEIGHT * 10, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);

  m->renderer = SDL_CreateRenderer(m->window, 
    -1, SDL_RENDERER_ACCELERATED);// | SDL_RENDERER_PRESENTVSYNC);

  SDL_RenderSetLogicalSize(m->renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);

  m->texture = SDL_CreateTexture(m->renderer,
    SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
    DISPLAY_WIDTH, DISPLAY_HEIGHT);

  return m;
}

static void machine_Tick(Machine *m) {
  static SDL_Event event;
  static int pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];
  static Uint64 oldTime = 0;
  static Uint64 newTime = 0;

  m->sys.btns = 0x00;
  memset(m->sys.tbuf, 0, sizeof(m->sys.tbuf));

  while (SDL_PollEvent(&event))
  {
      switch(event.type)
      {
        case SDL_QUIT:
          m->sys.shutdown = true;
          break;
        case SDL_KEYUP:
          if (event.key.keysym.sym == SDLK_ESCAPE) m->sys.btns |= BTN_ESCP;
          break;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_UP)    m->sys.btns |= BTN_UP;
          if (event.key.keysym.sym == SDLK_DOWN)  m->sys.btns |= BTN_DOWN;
          if (event.key.keysym.sym == SDLK_LEFT)  m->sys.btns |= BTN_LEFT;
          if (event.key.keysym.sym == SDLK_RIGHT) m->sys.btns |= BTN_RIGHT;
          if (event.key.keysym.sym == SDLK_z)     m->sys.btns |= BTN_A;
          if (event.key.keysym.sym == SDLK_x)     m->sys.btns |= BTN_B;
          if (event.key.keysym.sym == SDLK_BACKSPACE) m->sys.btns |= BTN_BKSP;
          if (event.key.keysym.sym == SDLK_RETURN) {
              m->sys.btns |= BTN_RETN;
              m->sys.btns |= BTN_A;
          }
          break;
        case SDL_TEXTINPUT:
            strncpy(m->sys.tbuf, event.text.text, sizeof(m->sys.tbuf) - 1);
            break;
      }
  }

  newTime = SDL_GetTicks();
  float delta = (float)(newTime - oldTime) / 1000.0f;
  oldTime = newTime;

  m->sys.delta = delta;
  system_Tick(&m->sys);

  SDL_SetRenderDrawColor(m->renderer,
    0,
    0,
    0,
    SDL_ALPHA_OPAQUE);

  SDL_RenderClear(m->renderer);

  int pixel = 0;
  for (int y = 0; y < DISPLAY_HEIGHT; ++y)
  for (int x = 0; x < DISPLAY_WIDTH; ++x)
  {
    bool on = (m->sys.dsp.fbuf[x / 8][y] << (x % 8)) & 0x80;
    pixels[pixel] = on ? m->sys.dsp.fgcol.value : m->sys.dsp.bgcol.value;
    pixel++;
  }

  SDL_UpdateTexture(m->texture, NULL, pixels, DISPLAY_WIDTH * 4);
  SDL_RenderCopy(m->renderer, m->texture, NULL, NULL);
  SDL_RenderPresent(m->renderer);
}

static void machine_Destroy(Machine *m) {
  SDL_DestroyTexture(m->texture);
  SDL_DestroyRenderer(m->renderer);  
  SDL_DestroyWindow(m->window);
  memset(m, 0, sizeof(Machine));
  free(m);
}

/* ------------------------------------------------------------------------- */
// COMPONENT CODE FORWARD DECLARATIONS
/* ------------------------------------------------------------------------- */

static void playGameComp_Exe(Component *, System *);
static void newGameComp_Exe(Component *, System *);
static void editGameComp_Exe(Component *, System *);

/* ------------------------------------------------------------------------- */
// MAIN
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv) {
  Machine *m = machine_Create();

  Module *module = module_Create();
  module_AddComponent(module, component_Create("PLAY", playGameComp_Exe), component_Destroy);
  module_AddComponent(module, component_Create("EDIT", editGameComp_Exe), component_Destroy);
  module_AddComponent(module, component_Create("NEW", newGameComp_Exe), component_Destroy);
  system_PushModule(&m->sys, module, module_Destroy);

  while (!m->sys.shutdown) machine_Tick(m);

  machine_Destroy(m);
  exit(EXIT_SUCCESS);
}

/* ------------------------------------------------------------------------- */
// COMPONENTS
/* ------------------------------------------------------------------------- */

static void playGameComp_Exe(Component *cmp, System *sys) {
  Menu *menu = (Menu *)cmp->data;
  if (!menu->initialized) {
  }

  menu_HandleInput(menu, sys);
  menu_Draw(menu, sys);
  display_RCBUF(&sys->dsp);
}

static void newGameComp_Exe(Component *cmp, System *sys) {
  static void createGameMenuOption_OnPick(MenuItem *item, System *sys);
  Menu *menu = (Menu *)cmp->data;
  if (!menu->initialized) {
    menu->count = 1;
    strcpy(menu->items[0].name, "Create Game");
    menu->items[0].onPick = createGameMenuOption_OnPick;
    menu->initialized = true;
  }
  menu_HandleInput(menu, sys);
  menu_Draw(menu, sys);
  display_RCBUF(&sys->dsp);
}

static void createGameMenuOption_OnPick(MenuItem *item, System *sys) {
  static void createGameComp_Exe(Component *, System *);
  Module *module = module_Create();
  module_AddComponent(module, component_Create("CREATE GAME", createGameComp_Exe), component_Destroy);
  system_PushModule(sys, module, module_Destroy);
}

static void createGameComp_Exe(Component *cmp, System *sys) {
  static void createGameNameTextInput_OnReturn(const char *input, System *sys);
  TextInputBuffer *inp = (TextInputBuffer *)cmp->data;
  if (!inp->initialized) {
    inp->startRow = 1;
    inp->startCol = 5;
    inp->maxlen = 15;
    inp->count = 0;
    inp->cursorOn = true;
    inp->cursorTimer = 0.0f;
    inp->onReturn = createGameNameTextInput_OnReturn;
    inp->initialized = true;
  }
  display_STR(&sys->dsp, 1, 0, "Name=", STR_FLAG_NONE);
  textInputBuffer_HandleInput(inp, sys);
  textInputBuffer_Draw(inp, sys);
  display_RCBUF(&sys->dsp);
}

static void createGameNameTextInput_OnReturn(const char *input, System *sys) {
  static void gameEditorComp_Exe(Component *, System *);

  Game *game = (Game *)sys->fswap;
  memset(game, 0, sizeof(Game));
  strncpy(game->assets.name, input, sizeof(game->assets.name) - 1);
  memcpy(game->assets.font, font_rom, sizeof(game->assets.font));

  Module *module = module_Create();
  char cmpName[16] = {'\0'};
  strcpy(cmpName, "GAME:");
  strncat(cmpName, input, sizeof(cmpName) - 1);
  module_AddComponent(module, component_Create(cmpName, gameEditorComp_Exe), component_Destroy);
  system_PopModule(sys);
  system_PushModule(sys, module, module_Destroy);
}

static void gameEditorComp_Exe(Component *cmp, System *sys) {
  Menu *menu = (Menu *)cmp->data;
  if (!menu->initialized) {
    byte itemCnt = 0;
    strncpy(menu->items[itemCnt++].name, "Play", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Code Editor", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Sprite Editor", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Font Editor", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Save and Exit", sizeof(((MenuItem *)0)->name) - 1);
    menu->count = itemCnt;
    menu->initialized = true;
  }
  

  menu_HandleInput(menu, sys);
  menu_Draw(menu, sys);
  display_RCBUF(&sys->dsp);
}

static void editGameComp_Exe(Component *cmp, System *sys) {
}