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

#define DISK_FILE_NAME          "fc85.disk"
#define DISK_SIZE               163840
#define DISK_BLOCK_SIZE         640
#define DISK_BLOCK_COUNT        (DISK_SIZE/DISK_BLOCK_SIZE)
#define DISK_FILE_SIZE_MAX      DISK_FILE_MAX_BLOCKS*DISK_BLOCK_SIZE
#define DISK_FILE_NAME_SIZE     16
#define DISK_FILE_MAX_BLOCKS    32
#define DISK_FLAG_NONE          0
#define DISK_FLAG_WRITE         0x80
#define DISK_FLAG_READ          0x40

#define msizeof(type, member) sizeof(((type *)0)->member)

/* ------------------------------------------------------------------------- */
// Types
/* ------------------------------------------------------------------------- */

typedef unsigned char byte;

typedef unsigned short word;

typedef unsigned int dword;

typedef union {
  int value;
  struct {
    byte b;
    byte g;
    byte r;
  } rgb;
} Color;

typedef struct {
  void *data;
  void (*tick)(void *, void *);
  void (*destroy)(void *);
} Process;

typedef struct {
  struct {
    struct _sys {
      byte flags;
    } sys;
    struct _disp {
      byte flags;
      Color foreground;
      Color background;
      byte buffer[DISP_HEIGHT_PIXELS][DISP_WIDTH_PIXELS/DISP_PIXELS_PER_BYTE];
      struct {
        byte value;
        byte flags;
      } charCells[DISP_CHAR_CELL_ROWS][DISP_CHAR_CELL_COLS];
    } disp;
    struct _disk {
      byte flags;
      byte name[DISK_FILE_NAME_SIZE];
      byte buffer[DISK_FILE_SIZE_MAX];
    } disk;
    struct _inpt {
      word btns;
      byte text[16];
    } inpt;
    byte appl[SYS_MEMORY - (sizeof(struct _sys) + sizeof(struct _disp) + sizeof(struct _disk) + sizeof(struct _inpt))];
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
// Process Headers
/* ------------------------------------------------------------------------- */

#undef FC85_PROC_IMPLEMENTATIONS
#include "proc_menu.h"
#include "proc_sys.h"

/* ------------------------------------------------------------------------- */
// Globals
/* ------------------------------------------------------------------------- */

static FC85 *_fc85 = NULL;

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

static void system_PushProc(System *self, void *data, void (*tick)(void *, void *), void (*destroy)(void *)) 
{
  assert(self->procCount < SYS_NUM_PROCESSES);
  Process *procSlot = (Process *)&self->procStack[self->procCount];
  self->procCount++;

  memset(procSlot, 0, sizeof(Process));
  procSlot->data = data;
  procSlot->tick = tick;
  procSlot->destroy = destroy;
}

static void system_PopProc(System *self)
{
  assert(self->procCount > 0);
  assert(self->deadProcCount < SYS_NUM_PROCESSES);
  memcpy(&self->deadProcStack[self->deadProcCount],
    &self->procStack[self->procCount],
    sizeof(Process));
  self->procCount--;
  self->deadProcCount++;
}

static void system_Boot(System *self) 
{
  memset(self, 0, sizeof(System));
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

  if (self->procCount > 0) 
  {
    Process *proc = (Process *)&self->procStack[self->procCount - 1];
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

void displayDevice_Refresh(DisplayDevice *self, System *sys) 
{
  static int pixels[DISP_WIDTH_PIXELS * DISP_HEIGHT_PIXELS];

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
}

static void diskDevice_Read(DiskDevice *self, System *sys)
{
}

static void diskDevice_Refresh(DiskDevice *self, System *sys)
{
  if (sys->mem.disk.flags & DISK_FLAG_WRITE) diskDevice_Write(self, sys);
  else if (sys->mem.disk.flags & DISK_FLAG_READ) diskDevice_Read(self, sys);
  sys->mem.disk.flags = DISK_FLAG_NONE;
}

/* ------------------------------------------------------------------------- */
// InputDevice
/* ------------------------------------------------------------------------- */

static void inputDevice_Refresh(InputDevice *self, System *sys) 
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
  FC85 *fc85 = fc85_Get();
  inputDevice_Refresh(&fc85->inpt, &fc85->sys);
  system_Tick(&fc85->sys);
  displayDevice_Refresh(&fc85->disp, &fc85->sys);
  diskDevice_Refresh(&fc85->disk, &fc85->sys);
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
