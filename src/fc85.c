// fc85 - A Fantasy Console developed for #FCDEV_JAM 2017
// Created by Shawn Rakowski

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <windows.h>

#include "rom.h"

#define TokenType MyTokenType

#define BGCOL             0xFF5C4530
#define FGCOL             0xFF99C278

#define NUM_SPRITES           256
#define DISK_SIZE             163840
#define DISK_BLOCK_SIZE       640
#define MAX_FILE_SIZE         20480
#define DISK_BLOCK_COUNT      (DISK_SIZE/DISK_BLOCK_SIZE)

#define COMPONENT_MEMORY_SIZE 16384
#define LUA_CODE_SIZE         65536

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
#define cast(type, var) ((type)(var))

#define RUN_FUNC_NAME        "fc85run"
#define DELTA_NAME           "fc85delta"
#define YIELD_FUNC_NAME      "fc85yield"

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

typedef enum {
  TOKEN_INVALID = 0,
  TOKEN_NUMBER,
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NOT,
  TOKEN_STO,
  TOKEN_AND,
  TOKEN_DASH,
  TOKEN_PLUS,
  TOKEN_MULT,
  TOKEN_DIVD,
  TOKEN_LT,
  TOKEN_GT,
  TOKEN_EQ,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_COMMA,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
} TokenType;

typedef struct {
  byte initialized;
  byte done;
  lua_State *lua;
} Interpreter;

typedef struct {
  byte initialized;
  int fileHead;
  int fileLine;
  int fileChar;
  byte cursorOn;
  float cursorTimer;
  word codePos;
  byte code[sizeof(((Game *)0)->code)];
} CodeEditor;

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
  byte data[COMPONENT_MEMORY_SIZE];
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
  byte isGame;
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
  byte buf[CHAR_CELL_ROWS][CHAR_CELL_COLS];
  byte line;
} Home;

typedef struct {
  byte blocks[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];
} DiskMap;

typedef struct {
  int size;
  byte name[16];
  byte blocks[32];
  byte ext[10]; //extension
  byte blockCount;
  byte init;
} DiskFile;

// 19x64 + 32 + 31 + 1 = 1280
// 1280 = 2x640 = 254 blocks for user space
typedef struct {
  byte blocks[32]; // bitmap of used blocks
  DiskFile files[19];
  byte ext[31]; // for extension
  byte init;
} DiskHeader; 

typedef struct {
  Display dsp;
  Home home;
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
  byte disk[DISK_SIZE];
} System;

typedef struct Machine {
  System sys;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
} Machine;

Machine *_machine;

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

static byte display_GETCH(Display *dsp, byte row, byte col) {
  assert(dsp);
  return dsp->cbuf[row][col].c;
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
// Home
/* ------------------------------------------------------------------------- */

void home_Reset(Home *home, System *sys) {
  memset(home, 0, sizeof(home));
}

void home_Disp(Home *home, System *sys, const char *line) {
  
  char buffer[sizeof(home->buf[home->line]) + 1] = { '\0' };
  strncpy(buffer, line, sizeof(home->buf[home->line]));
  memcpy(home->buf[home->line], buffer, sizeof(home->buf[home->line]));
  home->line++;

  if (home->line == CHAR_CELL_ROWS)
  {
    for (int l = 0; l < CHAR_CELL_ROWS; l++) {
      memset(home->buf[l], 0, sizeof(home->buf[l]));
      if (l != CHAR_CELL_ROWS - 1) {
        memcpy(home->buf[l], home->buf[l + 1], sizeof(home->buf[l]));
      }
    }
    home->line--;
  }

  for (int l = 0; l < CHAR_CELL_ROWS; l++) {
    for (int c = 0; c < CHAR_CELL_COLS; c++) {
      display_CH(&sys->dsp, l, c, home->buf[l][c], CH_FLAG_NONE);
    }
  }

  printf("display %s", line);
  display_RCBUF(&sys->dsp);
}

/* ------------------------------------------------------------------------- */
// Code to call!
/* ------------------------------------------------------------------------- */

static int Disp(lua_State *L)
{
    int argc = lua_gettop(L);
    printf("Disp called %d args\n", argc);
    for (int i = 1; i < argc + 1; i++) {
      const char *val = luaL_checkstring(L, i);
      home_Disp(&_machine->sys.home,
        &_machine->sys,
        val);
    }
    // int x = (int)luaL_checkinteger(L, 1);
    // int y = (int)luaL_checkinteger(L, 2);
    // const char *val = luaL_checkstring(L, 3);
    // bool invert = false;
    // if (argc >= 4)
    // {
    //     invert = (bool)lua_toboolean(L, 4);
    // }
    // out(x, y, val, invert);
    return 0;
}

// static void Tick(lua_State *L, lua_Debug *ar) {
//   Sleep(100);
//   static void machine_Tick(Machine *m);
//   machine_Tick(_machine);
//   if (_machine->sys.shutdown) {
//   }
// }

/* ------------------------------------------------------------------------- */
// Interpreter
/* ------------------------------------------------------------------------- */

static int fc85yield(lua_State* L) {
	return lua_yield(L, 0);
}

static void interpreter_Init(Interpreter *ctx, System *sys, const char *code) {
  static const char *tokenize(const char *code, TokenType *type);
  static void system_PopModule(System *sys);

  char *tcode = (char *)calloc(1, LUA_CODE_SIZE);
  char lineBuffer[256] = {'\0'};
  char swapBuffer[256] = {'\0'};
  char *buffer = lineBuffer;
  bool closeParen = false;
  bool closeLbl = false;

  // IT'S TRANSPILER TIME!

  strncat(tcode, "function "RUN_FUNC_NAME"()\n", LUA_CODE_SIZE - strlen(tcode) - 1);

  memset(lineBuffer, 0, sizeof(lineBuffer));
  TokenType type = TOKEN_INVALID;
  const char *token = NULL;
  token = tokenize(code, &type);
  while (token != NULL && type != TOKEN_INVALID) {
    printf("%d/%s/%s\n", type, token, closeParen ? "await)" : "");
    switch (type)
    {
      case TOKEN_STO:
        if (closeParen) {
          strncat(buffer, ")", LUA_CODE_SIZE - strlen(tcode) - 1);
          closeParen = false;
        }
        buffer = swapBuffer;
        break;

      case TOKEN_COLON:
        if (strlen(swapBuffer) > 0) {
          strncat(tcode, swapBuffer, LUA_CODE_SIZE - strlen(tcode) - 1);
          strncat(tcode, " = ", LUA_CODE_SIZE - strlen(tcode) - 1);
        }
        if (closeParen) {
          strncat(lineBuffer, ")", LUA_CODE_SIZE - strlen(tcode) - 1);
          closeParen = false;
        } else if (closeLbl) {
          strncat(lineBuffer, "::", LUA_CODE_SIZE - strlen(tcode) - 1);
          closeLbl = false;
        }
        strncat(tcode, lineBuffer, LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(tcode, "\n", LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(tcode, YIELD_FUNC_NAME"()\n", LUA_CODE_SIZE - strlen(tcode) - 1);
        memset(lineBuffer, 0, sizeof(lineBuffer));
        memset(swapBuffer, 0, sizeof(swapBuffer));
        buffer = lineBuffer;
        break;

      case TOKEN_STRING:
        strncat(buffer, " \"", LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(buffer, token, LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(buffer, "\" ", LUA_CODE_SIZE - strlen(tcode) - 1);
        break;

      case TOKEN_IDENTIFIER:
        strncat(buffer, " ", LUA_CODE_SIZE - strlen(tcode) - 1);
        if (strcmp(token, "Disp") == 0) {
          strncat(buffer, token, LUA_CODE_SIZE - strlen(tcode) - 1);
          strncat(buffer, "(", LUA_CODE_SIZE - strlen(tcode) - 1);
          closeParen = true;
        } else if (strcmp(token, "Lbl") == 0) {
          strncat(buffer, "::", LUA_CODE_SIZE - strlen(tcode) - 1);
          closeLbl = true;
        } else if (strcmp(token, "Goto") == 0) {
          strncat(buffer, "goto", LUA_CODE_SIZE - strlen(tcode) - 1);
          strncat(buffer, " ", LUA_CODE_SIZE - strlen(tcode) - 1);
        } else {
          strncat(buffer, token, LUA_CODE_SIZE - strlen(tcode) - 1);
          strncat(buffer, " ", LUA_CODE_SIZE - strlen(tcode) - 1);
        }
        break;

      default:
        strncat(buffer, " ", LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(buffer, token, LUA_CODE_SIZE - strlen(tcode) - 1);
        strncat(buffer, " ", LUA_CODE_SIZE - strlen(tcode) - 1);
        break;
    }
    token = tokenize(NULL, &type);
  }

  if (strlen(swapBuffer) > 0) {
    strncat(tcode, swapBuffer, LUA_CODE_SIZE - strlen(tcode) - 1);
    strncat(tcode, " = ", LUA_CODE_SIZE - strlen(tcode) - 1);
  }
  if (closeParen) {
    strncat(lineBuffer, ")", LUA_CODE_SIZE - strlen(tcode) - 1);
    closeParen = false;
  } else if (closeLbl) {
    strncat(lineBuffer, "::", LUA_CODE_SIZE - strlen(tcode) - 1);
    closeLbl = false;
  }
  strncat(tcode, lineBuffer, LUA_CODE_SIZE - strlen(tcode) - 1);
  strncat(tcode, "\nend\n", LUA_CODE_SIZE - strlen(tcode) - 1);

  printf("%s", tcode);

  home_Reset(&sys->home, sys);
  
  ctx->lua = luaL_newstate();
  luaL_openlibs(ctx->lua);
  
  lua_register(ctx->lua, YIELD_FUNC_NAME, fc85yield);
  
  lua_pushcfunction(ctx->lua, Disp);
  lua_setglobal(ctx->lua, "Disp");
  // // lua_pushcfunction(L, lclr);
  // // lua_setglobal(L, "clr");

  if (luaL_loadstring(ctx->lua, tcode) != 0) {
    // do error?
    exit(EXIT_FAILURE);
  }

  free(tcode);

  lua_pcall(ctx->lua, 0, LUA_MULTRET, 0);
  
  // int count = 0;
  // lua_sethook(ctx->lua, Tick, LUA_MASKLINE, count);

  lua_getglobal(ctx->lua, RUN_FUNC_NAME);
  // lua_pcall(ctx->lua, 0, 0, 0);

  ctx->initialized = true;
  ctx->done = false;
}

static const char *tokenize(const char *code, TokenType *type)
{
  static char token[256] = { '\0' };
  static char *token_ptr = token;
  static const char *scanner = NULL;

  memset(token, 0, sizeof(token));
  token_ptr = token;
  *type = TOKEN_INVALID;

  if (code != NULL)
    scanner = code;

  if (scanner == NULL)
    return NULL;

  while (isspace(*scanner) && *scanner != '\0')
    scanner++;

  if (*scanner == '\0')
    return NULL;

  if (isdigit(*scanner))
  {
    while (isdigit(*scanner)) // scan for number
    {
      *token_ptr = *scanner;
      token_ptr++;
      scanner++;
    }
    *type = TOKEN_NUMBER;
    return token;
  }

  if (isalpha(*scanner) || *scanner == '_')
  {
    while ((isalnum(*scanner) || *scanner == '_') && *scanner != '\0')
    {
      *token_ptr = *scanner;
      token_ptr++;
      scanner++;
    }

    *type = TOKEN_IDENTIFIER;

    return token;
  }

  if (*scanner == '"')
  {
    scanner++;
    while (*scanner != '"' && *scanner != '\n' && *scanner != '\0')
    {
      *token_ptr = *scanner;
      token_ptr++;
      scanner++;
    }

    scanner += (*scanner != '\0' ? 1 : 0);

    *type = TOKEN_STRING;
    return token;
  }

  if (strchr("!|&-+*/<>=;:(),", *scanner) != NULL)
  {
    *token_ptr = *scanner;
    *type = *scanner == '!' ? TOKEN_NOT
      : *scanner == '|' ? TOKEN_STO
      : *scanner == '&' ? TOKEN_AND
      : *scanner == '-' ? TOKEN_DASH
      : *scanner == '+' ? TOKEN_PLUS
      : *scanner == '*' ? TOKEN_MULT
      : *scanner == '/' ? TOKEN_DIVD
      : *scanner == '<' ? TOKEN_LT
      : *scanner == '>' ? TOKEN_GT
      : *scanner == '=' ? TOKEN_EQ
      : *scanner == ':' ? TOKEN_COLON
      : *scanner == ';' ? TOKEN_SEMICOLON
      : *scanner == '(' ? TOKEN_LEFT_PAREN
      : *scanner == ')' ? TOKEN_RIGHT_PAREN
      : *scanner == ',' ? TOKEN_COMMA
      : TOKEN_INVALID;
    scanner++;
    return token;
  }

  return token;
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
// CODE EDITOR
/* ------------------------------------------------------------------------- */

void codeEditor_HandleInput(CodeEditor *self, System *sys) {
  if (sys->btns & BTN_RETN) {
    self->code[self->codePos] = ':';
    self->codePos++;
  } else if (sys->btns & BTN_BKSP && self->codePos > 1) {
    self->codePos--;
    self->code[self->codePos] = '\0';
  }

  for (int c = 0; c < strlen(sys->tbuf); c++) {
    self->code[self->codePos] = sys->tbuf[c];
    self->codePos++;
  }

  self->cursorTimer += sys->delta;
  if (self->cursorTimer > 0.5f) {
    self->cursorOn = !self->cursorOn;
    self->cursorTimer = 0;
  }
}

void codeEditor_Draw(CodeEditor *self, System *sys) {
  int lineCount = 1;
  struct {
    byte *start;
    int len;
  } lines[sizeof(self->code) + 1];
  lines[0].start = self->code;
  int lineLen = 1;
  for (int c = 1; c < sizeof(self->code) && c < strlen(self->code); c++) {
    if (self->code[c] == ':') {
      lines[lineCount - 1].len = lineLen;
      lineLen = 0;
      lines[lineCount].start = &self->code[c];
      lineCount++;
    }
    lineLen++;
  }
  lines[lineCount - 1].len = lineLen;

  int row = 0;
  int col = 0;
  for (int l = self->fileHead; l < lineCount && row < CHAR_CELL_ROWS; l++) {
    row++;
    col = 0;
    for (int ch = 0; ch < lines[l].len; ch++) {
      byte val = lines[l].start[ch];
      display_CH(&sys->dsp, row, col, val, CH_FLAG_NONE);
      col++;
      if (col >= CHAR_CELL_COLS) {
        col = 0;
        row++;
        if (row >= CHAR_CELL_ROWS) {
          break;
        }
      }
    }
  }

  display_CH(&sys->dsp, row, col, 
    display_GETCH(&sys->dsp, row, col), 
    self->cursorOn ? CH_FLAG_NONE : CH_FLAG_INVERT);
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

static Module *module_Create(bool isGame) {
  Module *self = (Module *)calloc(1, sizeof(Module));
  self->isGame = isGame;
  self->cmpCnt = 0;
  return self;
}

static void module_Tick(Module *m, System *sys) {
  if (!m->isGame) {
    display_CLR(&sys->dsp, CLR_FLAG_ALL);

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
  }

  m->cmpLst[m->actvCmp].cmp->exe(
    m->cmpLst[m->actvCmp].cmp,
    sys
  );
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
// DISK
/* ------------------------------------------------------------------------- */

static void disk_Init(byte *disk) {
  printf("f:%d\n", (int)sizeof(DiskFile));
  printf("h:%d\n", (int)sizeof(DiskHeader));
  assert(sizeof(DiskHeader) == (DISK_BLOCK_SIZE * 2));
  memset(disk, 0, DISK_SIZE);
  DiskHeader *hdr = (DiskHeader *)disk;
  hdr->blocks[0] |= 0xC0; // 1100 0000 2 blocks for header
  hdr->init = true;
}

static void disk_Save(byte *disk, const char *fileName, byte *data, int size) {
  DiskMap *map = (DiskMap *)disk;
  DiskHeader *hdr = (DiskHeader *)disk;
  assert(hdr->init);
  assert(size <= MAX_FILE_SIZE);
  DiskFile *targetSlot = NULL;
  DiskFile *existingSlot = NULL;
  for (int i = 0; i < arraylen(hdr->files) && !existingSlot; i++) {
    if (!targetSlot && hdr->files[i].name[0] == '\0') {
      targetSlot = &hdr->files[i];
    }
    if (strncmp(hdr->files[i].name, fileName, sizeof(hdr->files[i].name)) == 0) {
      existingSlot = &hdr->files[i];
    }
  }

  if (existingSlot)
  {
    return;
  }

  assert(targetSlot);

  // find blocks
  memset(targetSlot->name, 0, sizeof(targetSlot->name));
  strncpy(targetSlot->name, fileName, sizeof(targetSlot->name) - 1);
  int blocksNeeded = (size / DISK_BLOCK_SIZE) + (((size % DISK_BLOCK_SIZE) > 0) ? 1 : 0);
  targetSlot->size = size;
  targetSlot->blockCount = blocksNeeded;
  for (byte b = 0; b < DISK_BLOCK_COUNT && blocksNeeded > 0; b++) {
    byte sector = b / 8;
    byte blockInSector = b % 8;
    byte mask = 0x80 >> blockInSector;
    if (!(hdr->blocks[sector] & mask)) {
      blocksNeeded--;
      targetSlot->blocks[blocksNeeded] = b;
    }
  }

  // todo: disk full?
  assert(blocksNeeded == 0);

  const char *dataPtr = data;
  int dataRemaining = size;
  for (byte b = 0; b < targetSlot->blockCount; b++) {
    // record block as used
    byte sector = targetSlot->blocks[b] / 8;
    byte blockInSector = targetSlot->blocks[b] % 8;
    byte mask = 0x80 >> blockInSector;
    hdr->blocks[sector] |= mask;

    // copy data to block
    byte *blockData = map->blocks[targetSlot->blocks[b]];
    memset(blockData, 0, DISK_BLOCK_SIZE);
    memcpy(blockData, dataPtr, min(DISK_BLOCK_SIZE, dataRemaining));

    // update positioning
    dataPtr += DISK_BLOCK_SIZE;
    dataRemaining -= DISK_BLOCK_SIZE;
  }
  printf("dr:%d\n", dataRemaining);
  assert(dataRemaining <= 0);
}

DiskFile *disk_GetFile(byte *disk, bool reset) {
  static byte file = 0;
  if (reset) {
    file = 0;
  }
  DiskFile *data = NULL;
  DiskHeader *hdr = (DiskHeader *)disk;
  while (file < arraylen(hdr->files) && data == NULL) {
    if (hdr->files[file].name[0] != 0) {
      data = &hdr->files[file];
    }
    file++;
  }
  return data;
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

  if (sys->btns & BTN_ESCP) {
    system_PopModule(sys);
    return;
  }

  Module* m = sys->moduleStack[sys->count - 1].mdl;

  module_Tick(m, sys);

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

  disk_Init(m->sys.disk);

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
  _machine = machine_Create();

  Module *module = module_Create(false);
  module_AddComponent(module, component_Create("PLAY", playGameComp_Exe), component_Destroy);
  module_AddComponent(module, component_Create("EDIT", editGameComp_Exe), component_Destroy);
  module_AddComponent(module, component_Create("NEW", newGameComp_Exe), component_Destroy);
  system_PushModule(&_machine->sys, module, module_Destroy);

  while (!_machine->sys.shutdown) machine_Tick(_machine);

  machine_Destroy(_machine);
  exit(EXIT_SUCCESS);
}

/* ------------------------------------------------------------------------- */
// COMPONENTS
/* ------------------------------------------------------------------------- */

static void playGameComp_Exe(Component *cmp, System *sys) {
  Menu *menu = (Menu *)cmp->data;

  for (int m = 0; m < arraylen(menu->items); m++) {
    memset(&menu->items[m], 0, sizeof(menu->items[m]));
  }

  byte itemCnt = 0;
  DiskFile *fd = disk_GetFile(sys->disk, true);
  while (fd != NULL)
  {
    char buffer[256] = {'\0'};
    strncpy(buffer, fd->name, sizeof(fd->name));
    strncpy(menu->items[itemCnt++].name, buffer, sizeof(((MenuItem *)0)->name) - 1);
    fd = disk_GetFile(sys->disk, false);
  }
  menu->count = itemCnt;

  menu_HandleInput(menu, sys);
  menu_Draw(menu, sys);
  display_RCBUF(&sys->dsp);
}

static void editGameComp_Exe(Component *cmp, System *sys) {
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
  Module *module = module_Create(false);
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
  strcpy(game->code, ":");

  #pragma warning (disable : 4267)
  int fileSize = (int)sizeof(game->assets) + strlen(game->code);

  disk_Save(sys->disk, input, (byte *)game, fileSize);

  Module *module = module_Create(false);
  char cmpName[32] = {'\0'};
  strcpy(cmpName, "GAME:");
  strncat(cmpName, input, sizeof(cmpName) - 1);
  module_AddComponent(module, component_Create(cmpName, gameEditorComp_Exe), component_Destroy);
  system_PopModule(sys);
  system_PushModule(sys, module, module_Destroy);
}

static void gameEditorComp_Exe(Component *cmp, System *sys) {
  static void playMenuOption_OnPick(MenuItem *item, System *sys);
  static void editCodeMenuOption_OnPick(MenuItem *item, System *sys);
  static void editSpritesMenuOption_OnPick(MenuItem *item, System *sys);
  static void editFontMenuOption_OnPick(MenuItem *item, System *sys);

  Menu *menu = (Menu *)cmp->data;
  if (!menu->initialized) {
    byte itemCnt = 0;
    menu->items[itemCnt].onPick = editCodeMenuOption_OnPick;
    strncpy(menu->items[itemCnt++].name, "Edit Code", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Edit Sprites", sizeof(((MenuItem *)0)->name) - 1);
    strncpy(menu->items[itemCnt++].name, "Edit Font", sizeof(((MenuItem *)0)->name) - 1);
    menu->items[itemCnt].onPick = playMenuOption_OnPick;
    strncpy(menu->items[itemCnt++].name, "Play", sizeof(((MenuItem *)0)->name) - 1);
    menu->count = itemCnt;
    menu->initialized = true;
  }

  menu_HandleInput(menu, sys);
  menu_Draw(menu, sys);
  display_RCBUF(&sys->dsp);
}

static void playMenuOption_OnPick(MenuItem *item, System *sys) {
  static void playComp_Exe(Component *cmp, System *sys);
  Module *module = module_Create(true);
  char cmpName[32] = {'\0'};
  module_AddComponent(module, component_Create("", playComp_Exe), component_Destroy);
  system_PushModule(sys, module, module_Destroy);
}

static void editCodeMenuOption_OnPick(MenuItem *item, System *sys) {
  static void codeEditorComp_Exe(Component *, System *);
  Module *module = module_Create(false);
  char cmpName[32] = {'\0'};
  strcpy(cmpName, "CODE:");
  strncat(cmpName, ((Game *)sys->fswap)->assets.name, sizeof(cmpName) - 1);
  module_AddComponent(module, component_Create(cmpName, codeEditorComp_Exe), component_Destroy);
  system_PushModule(sys, module, module_Destroy);
}

static void codeEditorComp_Exe(Component *cmp, System *sys) {
  CodeEditor *code = (CodeEditor *)cmp->data;
  if (!code->initialized) {
    code->fileHead = 0;
    code->fileLine = 0;
    code->fileChar = 0;
    code->codePos = 1;
    memcpy(code->code, cast(Game *, sys->fswap)->code, sizeof(code->code));
    code->initialized = true;
  }

  codeEditor_HandleInput(code, sys);
  codeEditor_Draw(code, sys);
  display_RCBUF(&sys->dsp);

  memcpy(((Game *)(sys->fswap))->code, code->code, sizeof(code->code));
}

static void playComp_Exe(Component *cmp, System *sys) {
  Interpreter *ctx = cast(Interpreter *, cmp->data);
  if (!ctx->initialized) {
    interpreter_Init(ctx, sys, cast(Game *, sys->fswap)->code);
  }

  if (!ctx->done) {

    int ret = lua_resume(ctx->lua, NULL, 0);

    if (ret == LUA_YIELD) {
      printf("[ C ] Lua has yielded!\n");
    } else if (ret == 0) {
      lua_close(ctx->lua);
      printf("[ C ] Lua has finished!\n");
      home_Disp(&sys->home, sys, "Done");
      ctx->done = true;
    } else {
      home_Disp(&sys->home, sys, "ERROR!");
    }
  }
}
// static void xMenuOption_OnPick(MenuItem *item, System *sys) {
// }