// FC-85 - A Fantasy Console developed for #FCDEV_JAM 2017
// Created by Shawn Rakowski


/* ------------------------------------------------------------------------- */
// Includes
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* ------------------------------------------------------------------------- */
// Macros
/* ------------------------------------------------------------------------- */

#define SYS_MEMORY              65536
#define SYS_FLAG_SHUTDOWN       0x80
#define SYS_NUM_PROCESSES       8

#define DISP_WIDTH_PIXELS       96
#define DISP_HEIGHT_PIXELS      64
#define DISP_PIXELS_PER_BYTE    8
#define DISP_CHAR_WIDTH_PIXELS  6
#define DISP_CHAR_HEIGHT_PIXELS 8
#define DISP_CHAR_CELL_ROWS     (DISP_HEIGHT_PIXELS/DISP_CHAR_HEIGHT_PIXELS)
#define DISP_CHAR_CELL_COLS     (DISP_WIDTH_PIXELS/DISP_CHAR_WIDTH_PIXELS)
#define DISP_FLAG_NONE          0x00
#define DISP_FLAG_INVERT        0x01
#define DISP_FLAG_CHAR_MODE     0x02
#define DISP_DEFAULT_FG_COL     0xFF78C299
#define DISP_DEFAULT_BG_COL     0xFF5C4530

#define INPT_BTN_POWR           0x0800
#define INPT_BTN_ESCP           0x0400
#define INPT_BTN_RETN           0x0200
#define INPT_BTN_BKSP           0x0100
#define INPT_BTN_UP             0x0080
#define INPT_BTN_DOWN           0x0040
#define INPT_BTN_LEFT           0x0020
#define INPT_BTN_RIGHT          0x0010
#define INPT_BTN_START          0x0008
#define INPT_BTN_SELECT         0x0004
#define INPT_BTN_A              0x0002
#define INPT_BTN_B              0x0001

#define HOME_INPUT_BUFFER_SIZE  256

#define DISK_FILE_NAME          "fc85.disk"
#define DISK_SIZE               163840
#define DISK_BLOCK_SIZE         640
#define DISK_BLOCK_COUNT        (DISK_SIZE/DISK_BLOCK_SIZE)
#define DISK_FILE_SIZE_MAX      DISK_FILE_MAX_BLOCKS*DISK_BLOCK_SIZE
#define DISK_FILE_NAME_SIZE     16
#define DISK_FILE_MAX_BLOCKS    32
#define DISK_CODE_NONE           0
#define DISK_CODE_WRITE          1
#define DISK_CODE_READ           2
#define DISK_CODE_DIR            3

#define INTERRUPT_CODE_INVALID  0
#define INTERRUPT_CODE_DISK     1

#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define msizeof(type, member) sizeof(((type *)0)->member)

/* ------------------------------------------------------------------------- */
// Types
/* ------------------------------------------------------------------------- */

typedef unsigned char byte;
typedef signed char sbyte;

typedef unsigned short word;
typedef signed short sword;

typedef unsigned int dword;
typedef signed int sdword;

typedef union {
  dword value;
  struct {
    byte b;
    byte g;
    byte r;
    byte a;
  } rgb;
} Color;

typedef struct {
  struct _gameContent {
    byte name[DISK_FILE_NAME_SIZE];
    byte sprites[256][8];
    byte font[256][8];
  } content;
  byte code[DISK_FILE_SIZE_MAX-sizeof(struct _gameContent)];
} Game;

typedef struct {
  void *data;
  void (*tick)(void *, void *);
  void (*restore)(void *, void *);
  void (*destroy)(void *);
} Process;

typedef struct {
  struct {
    struct _sys {
      byte flags;
      float delta;
    } sys;
    struct _disp {
      byte flags;
      Color foreground;
      Color background;
      byte buffer[DISP_HEIGHT_PIXELS][DISP_WIDTH_PIXELS/DISP_PIXELS_PER_BYTE];
      struct _charCell {
        byte value;
        byte flags;
      } charCells[DISP_CHAR_CELL_ROWS][DISP_CHAR_CELL_COLS];
      byte font[256][8];
    } disp;
    struct _home {
      byte cursorRow;
      byte cursorCol;
      byte cursorOn;
      float cursorTimer;
      byte inputBuffer[HOME_INPUT_BUFFER_SIZE];
    } home;
    struct _disk {
      byte code;
      byte name[DISK_FILE_NAME_SIZE];
      byte buffer[DISK_FILE_SIZE_MAX];
    } disk;
    struct _inpt {
      word btns;
      byte text[16];
    } inpt;
    byte appl[SYS_MEMORY - (sizeof(struct _sys) + sizeof(struct _home) + sizeof(struct _disp) + sizeof(struct _disk) + sizeof(struct _inpt))];
  } mem;
  byte procCount;
  Process procStack[SYS_NUM_PROCESSES];
  byte deadProcCount;
  Process deadProcStack[SYS_NUM_PROCESSES];
} System;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
} DisplayDevice;

typedef union {
  byte raw[DISK_SIZE];
  byte blocks[DISK_BLOCK_COUNT][DISK_BLOCK_SIZE];
  struct _header {
    byte reserved[32];
    byte blockMap[DISK_BLOCK_COUNT/8]; // bitmap of used/available blocks
    struct _file {
      byte name[DISK_FILE_NAME_SIZE];
      byte blocks[DISK_FILE_MAX_BLOCKS];
      dword size;
      byte blockCount;
      byte reserved[11];
    } fileTable[((DISK_BLOCK_SIZE*2)/sizeof(struct _file))-1];
  } hdr;
} DiskDevice;

typedef struct {
  byte pass;
} InputDevice;

typedef struct {
  System sys;
  DisplayDevice disp;
  DiskDevice disk;
  InputDevice inpt;
} FC85;

/* ------------------------------------------------------------------------- */
// Globals/Misc
/* ------------------------------------------------------------------------- */

#include "font.h"
static FC85 *_fc85 = NULL;
static jmp_buf inputJmpBuf;
static bool system_IsShutdownFlagSet(System *self);
static void system_PushProc(System *self, void *data, void (*tick)(void *, void *), void (*restore)(void *, void *), void (*destroy)(void *));
static void system_PopProc(System *self);
static void tick();
static void diskDevice_Interrupt(DiskDevice *self, System *sys);
static FC85 *fc85_Get();

/* ------------------------------------------------------------------------- */
// System Api
/* ------------------------------------------------------------------------- */

static void _interrupt(System *sys, byte code)
{
  FC85 *fc85 = fc85_Get();
  switch (code)
  {
    case INTERRUPT_CODE_DISK:
      diskDevice_Interrupt(&fc85->disk, sys);
      break;
  }
}

static void _setPixel(System *sys, byte y, byte x, byte on)
{
  assert(sys);
  byte px = x / 8;
  byte bx = x % 8;
  if (on)
    sys->mem.disp.buffer[y][px] |= (0x80 >> bx);
  else
    sys->mem.disp.buffer[y][px] &= ((0x80 >> bx) ^ 0xFF);
}

static void _clrHome(System *sys)
{
  assert(sys);
  sys->mem.disp.flags |= DISP_FLAG_CHAR_MODE;
  memset(sys->mem.disp.charCells, 0, sizeof(sys->mem.disp.charCells));
  sys->mem.home.cursorRow = 0;
  sys->mem.home.cursorCol = 0;
}

static void _outputc(System *sys, byte row, byte col, byte value, byte flags)
{
  assert(sys);
  sys->mem.disp.flags |= DISP_FLAG_CHAR_MODE;
  if (row >= DISP_CHAR_CELL_ROWS || col >= DISP_CHAR_CELL_COLS) return;
  sys->mem.disp.charCells[row][col].value = value;
  sys->mem.disp.charCells[row][col].flags = flags;
}

static void _output(System *sys, byte row, byte col, byte *value, byte flags)
{
  assert(sys);
  sys->mem.disp.flags |= DISP_FLAG_CHAR_MODE;
  for (byte i = 0, c = col; i < strlen(value); i++, c++)
    _outputc(sys, row, c, value[i], flags);
}

static void _shiftUp(System *sys)
{
  if (sys->mem.home.cursorRow == DISP_CHAR_CELL_ROWS)
  {
    for (int r = 0; r < DISP_CHAR_CELL_ROWS - 1; r++)
    {
      memcpy(sys->mem.disp.charCells[r],
        sys->mem.disp.charCells[r + 1],
        sizeof(sys->mem.disp.charCells[r]));
    } 
    sys->mem.home.cursorRow--;
  }
}

static void _disp(System *sys, byte *value, bool line)
{
  assert(sys && value);
  for (int c = 0; c < (int)strlen(value); c++) 
  {
    _outputc(sys, 
      sys->mem.home.cursorRow, 
      sys->mem.home.cursorCol,
      value[c], DISP_FLAG_NONE);

    sys->mem.home.cursorCol++;
    if (sys->mem.home.cursorCol >= DISP_CHAR_CELL_COLS)
    {
      sys->mem.home.cursorRow++;
      sys->mem.home.cursorCol = 0;
    }

    _shiftUp(sys);
  }

  if (line) 
  {
    sys->mem.home.cursorCol = 0;
    sys->mem.home.cursorRow++;
    _shiftUp(sys);
  }
}

static void _pullInput(void *_, System *sys)
{
  byte *inp = sys->mem.home.inputBuffer;
  if (sys->mem.inpt.btns & INPT_BTN_BKSP && strlen(inp) > 0) 
  {
    inp[strlen(inp) - 1] = 0;
    _outputc(sys, sys->mem.home.cursorRow, sys->mem.home.cursorCol, 0, DISP_FLAG_NONE);
    sys->mem.home.cursorCol--;
    if (((sbyte)sys->mem.home.cursorCol) < 0)
    {
      sys->mem.home.cursorCol = DISP_CHAR_CELL_COLS - 1;
      sys->mem.home.cursorRow--;
    }
  }

  for (int c = 0; c < strlen(sys->mem.inpt.text); c++)
  {    
    _outputc(sys, sys->mem.home.cursorRow, sys->mem.home.cursorCol, 
      sys->mem.inpt.text[c], DISP_FLAG_NONE);
    sys->mem.home.cursorCol++;
    if (((sbyte)sys->mem.home.cursorCol) == DISP_CHAR_CELL_COLS)
    {
      sys->mem.home.cursorCol = 0;
      sys->mem.home.cursorRow++;
    }
    _shiftUp(sys);
  }

  strncat(inp, sys->mem.inpt.text, 
    (sizeof(sys->mem.home.inputBuffer) - strlen(inp) - 1));

  // cursor
  sys->mem.home.cursorTimer += sys->mem.sys.delta;
  if (sys->mem.home.cursorTimer > 0.5f) 
  {
    sys->mem.home.cursorOn = !sys->mem.home.cursorOn;
    sys->mem.home.cursorTimer = 0.0f;
  }
  _outputc(sys, sys->mem.home.cursorRow, sys->mem.home.cursorCol,
    219, sys->mem.home.cursorOn  ? DISP_FLAG_NONE : DISP_FLAG_INVERT);
}

static bool _awaitInput(System *sys)
{
  byte procCount = 0;
  procCount = sys->procCount;
  memset(sys->mem.home.inputBuffer, 0, sizeof(sys->mem.home.inputBuffer));
  system_PushProc(sys, NULL, _pullInput, NULL, NULL);
  while (!system_IsShutdownFlagSet(sys) &&
    !(sys->mem.inpt.btns & INPT_BTN_RETN) &&
    sys->procCount - 1 == procCount)
      tick();
  system_PopProc(sys);
  return procCount == sys->procCount;
}

static bool _input(System *sys, byte *prompt)
{
  assert(sys);
  _disp(sys, prompt ? prompt : "?", false);
  return _awaitInput(sys);
}

/* ------------------------------------------------------------------------- */
// Process and Data Headers
/* ------------------------------------------------------------------------- */

#undef FC85_PROC_IMPLEMENTATIONS
#include "proc_menu.h"
#include "proc_sys.h"
#include "proc_games.h"
#include "proc_create.h"
#include "proc_edit.h"
#include "proc_code.h"

/* ------------------------------------------------------------------------- */
// FC85
/* ------------------------------------------------------------------------- */

static FC85 *fc85_Create()
{
  FC85 *self = NULL;
  self = (FC85 *)calloc(1, sizeof(FC85));
  return self;
}

static void fc85_Destroy(FC85 *self) 
{
  memset(self, 0, sizeof(FC85));
  free(self);
}

static FC85 *fc85_Get()
{
  static void fc85_Cleanup(void);

  if (_fc85 == NULL)
  {
    _fc85 = fc85_Create();
    atexit(fc85_Cleanup);
  }

  assert(_fc85);
  return _fc85;
}

static void fc85_Cleanup(void)
{
  if (!_fc85)
    return;

  fc85_Destroy(_fc85);
  _fc85 = NULL;
}

/* ------------------------------------------------------------------------- */
// System
/* ------------------------------------------------------------------------- */

static void system_PushProc(System *self, void *data, void (*tick)(void *, void *), void (*restore)(void *, void *), void (*destroy)(void *))
{
  assert(self->procCount < SYS_NUM_PROCESSES);
  Process *procSlot = (Process *)&self->procStack[self->procCount];
  self->procCount++;

  memset(procSlot, 0, sizeof(Process));
  procSlot->data = data;
  procSlot->tick = tick;
  procSlot->restore = restore;
  procSlot->destroy = destroy;
}

static void system_PopProc(System *self)
{
  assert((sbyte)self->procCount > 0);
  assert(self->deadProcCount < SYS_NUM_PROCESSES);
  memcpy(&self->deadProcStack[self->deadProcCount],
    &self->procStack[self->procCount - 1],
    sizeof(Process));
  self->procCount--;
  self->deadProcCount++;

  if ((sbyte)self->procCount > 0)
    if (self->procStack[self->procCount - 1].restore)
    {
      self->procStack[self->procCount - 1].restore(
        self->procStack[self->procCount - 1].data,
        self
      );
    }
}

static void system_Boot(System *self) 
{
  memset(self, 0, sizeof(System));

  self->mem.disp.foreground.value = DISP_DEFAULT_FG_COL;
  self->mem.disp.background.value = DISP_DEFAULT_BG_COL;
  memcpy(self->mem.disp.font, font, 
    min(sizeof(self->mem.disp.font), sizeof(font)));

  sysProcess_Execute(self);
}

static bool system_IsShutdownFlagSet(System *self) 
{
  return self->mem.sys.flags & SYS_FLAG_SHUTDOWN;
}

static void system_SetShutdownFlag(System *self) 
{
  self->mem.sys.flags |= SYS_FLAG_SHUTDOWN;
}

static void system_Tick(System *self) 
{
  if (system_IsShutdownFlagSet(self))
    return;

  if (self->mem.inpt.btns & INPT_BTN_POWR) {
    system_SetShutdownFlag(self);
    return;
  }

  if (self->procCount > 1)
  {
    if (self->mem.inpt.btns & INPT_BTN_ESCP) {
      system_PopProc(self);
      return;
    }
  }

  if (self->procCount > 0) 
  {
    Process *proc = (Process *)&self->procStack[self->procCount - 1];
    if (proc->tick)
      proc->tick(proc->data, self);
  }

  while (self->deadProcCount > 0) 
  {
    self->deadProcCount--;
    Process *deadProc = (Process *)&self->deadProcStack[self->deadProcCount];
    if (deadProc->destroy) deadProc->destroy(deadProc->data);
    memset(deadProc, 0, sizeof(Process));
  }
}

/* ------------------------------------------------------------------------- */
// DisplayDevice
/* ------------------------------------------------------------------------- */

void displayDevice_Initialize(DisplayDevice *self) 
{
  int sdlInit = SDL_Init(SDL_INIT_VIDEO);
  assert(sdlInit >= 0);

  self->window = SDL_CreateWindow("FC-85",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    DISP_WIDTH_PIXELS * 10, DISP_HEIGHT_PIXELS * 10, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
  assert(self->window);

  self->renderer = SDL_CreateRenderer(self->window, 
    -1, SDL_RENDERER_ACCELERATED);// | SDL_RENDERER_PRESENTVSYNC);
  assert(self->renderer);

  SDL_RenderSetLogicalSize(self->renderer, DISP_WIDTH_PIXELS, DISP_HEIGHT_PIXELS);

  self->texture = SDL_CreateTexture(self->renderer,
    SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
    DISP_WIDTH_PIXELS, DISP_HEIGHT_PIXELS);
  assert(self->texture);
}

void displayDevice_Dispose(DisplayDevice *self) 
{
  SDL_DestroyTexture(self->texture);
  SDL_DestroyRenderer(self->renderer);  
  SDL_DestroyWindow(self->window);
  SDL_Quit();
}

void displayDevice_Interrupt(DisplayDevice *self, System *sys) 
{
  static int pixels[DISP_WIDTH_PIXELS * DISP_HEIGHT_PIXELS];

  if (sys->mem.disp.flags & DISP_FLAG_CHAR_MODE)
  {
    for (int r = 0; r < DISP_CHAR_CELL_ROWS; r++)
    for (int c = 0; c < DISP_CHAR_CELL_COLS; c++)
    {
      struct _charCell *cell = &sys->mem.disp.charCells[r][c];
      byte *fontChar = sys->mem.disp.font[cell->value];
      byte dy = r * DISP_CHAR_HEIGHT_PIXELS;
      byte dx = c * DISP_CHAR_WIDTH_PIXELS;
      for (int cy = 0; cy < DISP_CHAR_HEIGHT_PIXELS; cy++)
      for (int cx = 0; cx < DISP_CHAR_WIDTH_PIXELS; cx++)
      {
        byte on = (fontChar[cy] & (0x80 >> cx));
        on = (sys->mem.disp.flags & DISP_FLAG_INVERT) ? !on : on;
        on = (cell->flags & DISP_FLAG_INVERT) ? !on : on;
        _setPixel(sys, dy + cy, dx + cx, on);
      }
    }
  }

  SDL_SetRenderDrawColor(self->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(self->renderer);

  int pixel = 0;
  for (int y = 0; y < DISP_HEIGHT_PIXELS; ++y)
  for (int x = 0; x < DISP_WIDTH_PIXELS; ++x, pixel++)
  {
    bool on = (sys->mem.disp.buffer[y][x / 8] << (x % 8)) & 0x80;
    pixels[pixel] = on 
      ? sys->mem.disp.foreground.value 
      : sys->mem.disp.background.value;
  }

  SDL_UpdateTexture(self->texture, NULL, pixels, DISP_WIDTH_PIXELS * 4);
  SDL_RenderCopy(self->renderer, self->texture, NULL, NULL);
  SDL_RenderPresent(self->renderer);
}

/* ------------------------------------------------------------------------- */
// DiskDevice
/* ------------------------------------------------------------------------- */

static void diskDevice_Initialize(DiskDevice *self)
{
  FILE *fp = fopen(DISK_FILE_NAME, "rb");
  assert(sizeof(DiskDevice) == DISK_SIZE);
  assert(msizeof(DiskDevice, hdr) == (DISK_BLOCK_SIZE * 2));
  if (fp == NULL)
  {
    printf("[FC-85] no disk file "DISK_FILE_NAME", creating...\n");
    fp = fopen(DISK_FILE_NAME, "wb");
    assert(fp != NULL);
    memset(self, 0, sizeof(DiskDevice));
    self->hdr.blockMap[0] |= 0xC0; // 1100 0000 2 blocks for header
    for (int b = 0; b < sizeof(DiskDevice); b++)
      fputc(self->raw[b], fp);
    fclose(fp);
    printf("[FC-85] disk file created\n");
    fp = fopen(DISK_FILE_NAME, "rb");
  }
  assert(fp != NULL);
  printf("[FC-85] loading disk from "DISK_FILE_NAME"...\n");
  
  
  for (int b = 0; b < sizeof(DiskDevice); b++)
  {
    self->raw[b] = fgetc(fp);
    assert(self->raw[b] != EOF);
  }

  printf("[FC-85] disk loaded\n");
  fclose(fp);
}

static void diskDevice_Write(DiskDevice *self, System *sys)
{
  assert(self && sys);
  const byte *fileName = sys->mem.disk.name;
  const dword size = (dword)min(sizeof(sys->mem.disk.buffer), strlen(sys->mem.disk.buffer));
  const byte *data = sys->mem.disk.buffer;

  struct _file *targetSlot = NULL;
  struct _file *existingSlot = NULL;
  for (int i = 0; i < arraylen(self->hdr.fileTable); i++)
  {
    if (!targetSlot && self->hdr.fileTable[i].name[0] == '\0') 
    {
      targetSlot = &self->hdr.fileTable[i];
    }
    if (strncmp(self->hdr.fileTable[i].name, fileName, sizeof(self->hdr.fileTable[i].name)) == 0) 
    {
      existingSlot = &self->hdr.fileTable[i];
    }
  }

  if (existingSlot)
  {
    for (byte b = 0; b < existingSlot->blockCount; b++) 
    {
      byte block = existingSlot->blocks[b];
      byte sector = block / 8;
      byte blockInSector = block % 8;
      byte mask = 0x80 >> blockInSector;
      self->hdr.blockMap[sector] ^= mask;
    }
    memset(existingSlot, 0, sizeof(struct _file));
    targetSlot = existingSlot;
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
    if (!(self->hdr.blockMap[sector] & mask)) {
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
    self->hdr.blockMap[sector] |= mask;

    // copy data to block
    byte *blockData = self->blocks[targetSlot->blocks[b]];
    memset(blockData, 0, DISK_BLOCK_SIZE);
    memcpy(blockData, dataPtr, min(DISK_BLOCK_SIZE, dataRemaining));

    // update positioning
    dataPtr += DISK_BLOCK_SIZE;
    dataRemaining -= DISK_BLOCK_SIZE;
  }
  assert(dataRemaining <= 0);

  FILE *fp = fopen(DISK_FILE_NAME, "wb");
  assert(fp != NULL);
  for (int b = 0; b < sizeof(DiskDevice); b++)
    fputc(self->raw[b], fp);
  fclose(fp);
}

static void diskDevice_Read(DiskDevice *self, System *sys)
{
  const byte *fileName = sys->mem.disk.name;
  struct _file *fp = NULL;
  memset(sys->mem.disk.buffer, 0, sizeof(sys->mem.disk.buffer));
  for (int i = 0; i < arraylen(self->hdr.fileTable) && fp == NULL; i++)
    if (strncmp(self->hdr.fileTable[i].name, fileName,
      min(sizeof(self->hdr.fileTable[i].name), sizeof(sys->mem.disk.name))) == 0)
      fp = &self->hdr.fileTable[i];
  assert(fp);

  byte *bufferPtr = sys->mem.disk.buffer;
  dword bytesToRead = fp->size;
  for (byte b = 0; b < fp->blockCount; b++) 
  {
    const byte *blockData = self->blocks[fp->blocks[b]];
    memcpy(bufferPtr, blockData, min(bytesToRead, DISK_BLOCK_SIZE));
    bufferPtr += min(bytesToRead, DISK_BLOCK_SIZE);
    bytesToRead -= min(bytesToRead, DISK_BLOCK_SIZE);
  }
  assert((sdword)bytesToRead <= 0);
}

static void diskDevice_Dir(DiskDevice *self, System *sys)
{
  byte dirCnt = 0;
  struct _file *dir[arraylen(self->hdr.fileTable) + 1];
  memset(dir, 0, sizeof(dir));
  for (int i = 0; i < arraylen(self->hdr.fileTable); i++)
    if (self->hdr.fileTable[i].name[0] != '\0')
    {
      dir[dirCnt] = &self->hdr.fileTable[i];
      dirCnt++;
    }
  memcpy(sys->mem.disk.buffer, dir, 
    min(sizeof(dir), sizeof(sys->mem.disk.buffer)));
}

static void diskDevice_Interrupt(DiskDevice *self, System *sys)
{
  if (sys->mem.disk.code == DISK_CODE_WRITE) diskDevice_Write(self, sys);
  else if (sys->mem.disk.code == DISK_CODE_READ) diskDevice_Read(self, sys);
  else if (sys->mem.disk.code == DISK_CODE_DIR) diskDevice_Dir(self, sys);
  sys->mem.disk.code = DISK_CODE_NONE;
}

/* ------------------------------------------------------------------------- */
// InputDevice
/* ------------------------------------------------------------------------- */

static void inputDevice_Interrupt(InputDevice *self, System *sys) 
{
  static SDL_Event event;
  memset(&sys->mem.inpt, 0, sizeof(sys->mem.inpt));
  while (SDL_PollEvent(&event))
  {
      switch(event.type)
      {
        case SDL_QUIT:
          sys->mem.inpt.btns |= INPT_BTN_POWR;
          break;
        case SDL_KEYUP:
          if (event.key.keysym.sym == SDLK_ESCAPE) sys->mem.inpt.btns |= INPT_BTN_ESCP;
          break;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_UP)    sys->mem.inpt.btns |= INPT_BTN_UP;
          if (event.key.keysym.sym == SDLK_DOWN)  sys->mem.inpt.btns |= INPT_BTN_DOWN;
          if (event.key.keysym.sym == SDLK_LEFT)  sys->mem.inpt.btns |= INPT_BTN_LEFT;
          if (event.key.keysym.sym == SDLK_RIGHT) sys->mem.inpt.btns |= INPT_BTN_RIGHT;
          if (event.key.keysym.sym == SDLK_z)     sys->mem.inpt.btns |= INPT_BTN_A;
          if (event.key.keysym.sym == SDLK_x)     sys->mem.inpt.btns |= INPT_BTN_B;
          if (event.key.keysym.sym == SDLK_BACKSPACE) sys->mem.inpt.btns |= INPT_BTN_BKSP;
          if (event.key.keysym.sym == SDLK_RETURN) {
              sys->mem.inpt.btns |= INPT_BTN_RETN;
              sys->mem.inpt.btns |= INPT_BTN_A;
          }
          break;
        case SDL_TEXTINPUT:
            strncpy(sys->mem.inpt.text, event.text.text, sizeof(sys->mem.inpt.text) - 1);
            break;
      }
  }
}

/* ------------------------------------------------------------------------- */
// Game Loop
/* ------------------------------------------------------------------------- */

static void tick() 
{
  static Uint64 oldTime = 0;
  static Uint64 newTime = 0;
  FC85 *fc85 = fc85_Get();
  inputDevice_Interrupt(&fc85->inpt, &fc85->sys);
  newTime = SDL_GetTicks();
  float delta = (float)(newTime - oldTime) / 1000.0f;
  oldTime = newTime;
  fc85->sys.mem.sys.delta = delta;
  system_Tick(&fc85->sys);
  displayDevice_Interrupt(&fc85->disp, &fc85->sys);
  diskDevice_Interrupt(&fc85->disk, &fc85->sys);
}

/* ------------------------------------------------------------------------- */
// Entry Point
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv) 
{
  printf("[FC-85]  memory: total:%d, sys:%zd, appl:%zd\n", 
    SYS_MEMORY,
    SYS_MEMORY - msizeof(System, mem.appl),
    msizeof(System, mem.appl));

  FC85 *fc85 = fc85_Get();
  printf("[FC-85] initiating boot sequence...\n");
  printf("[FC-85] initializing display device...\n");
  displayDevice_Initialize(&fc85->disp);
  printf("[FC-85] initializing disk device...\n");
  diskDevice_Initialize(&fc85->disk);
  printf("[FC-85] initializing input device...\n");
  printf("[FC-85] system boot...\n");
  system_Boot(&fc85->sys);
  printf("[FC-85] boot sequence complete\n");

  while (!system_IsShutdownFlagSet(&fc85->sys)) tick();

  printf("[FC-85] initiating shutdown sequence...\n");
  printf("[FC-85] system shutdown...\n");
  printf("[FC-85] disposing input device...\n");
  printf("[FC-85] disposing disk device...\n");
  printf("[FC-85] disposing display device...\n");
  displayDevice_Initialize(&fc85->disp);
  printf("[FC-85] shutdown sequence complete\n");
  exit(EXIT_SUCCESS);
}

/* ------------------------------------------------------------------------- */
// Process Implementations
/* ------------------------------------------------------------------------- */

#define FC85_PROC_IMPLEMENTATIONS
#include "proc_menu.h"
#include "proc_sys.h"
#include "proc_games.h"
#include "proc_create.h"
#include "proc_edit.h"
#include "proc_code.h"
