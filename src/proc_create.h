#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_create_h_
#define _proc_create_h_
/* ------------------------------------------------------------------------- */

#include "proc_edit.h"

typedef struct {
  bool pass;
} CreateProcess;

static void createProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_create_c_
#define _proc_create_c_
/* ------------------------------------------------------------------------- */

static CreateProcess *createProcess_Create()
{
  CreateProcess *self = (CreateProcess *)calloc(1, sizeof(CreateProcess));
  assert(self);
  return self;
}

static void createProcess_Destroy(CreateProcess *self)
{
  assert(self);
  memset(self, 0, sizeof(CreateProcess));
  free(self);
}

static void createProcess_Tick(CreateProcess *self, System *sys)
{
  _clrHome(sys);
  _disp(sys, "GAME:CREATE", true);
  if (_input(sys, "Name="))
  {
    printf("[FC-85] creating %s\n", sys->mem.home.inputBuffer);
    memset(sys->mem.appl, 0, sizeof(sys->mem.appl));
    Game *game = (Game *)sys->mem.appl;
    strncpy(game->content.name, sys->mem.home.inputBuffer,
      sizeof(game->content.name) - 1);

    memset(sys->mem.disk.name, 0, sizeof(sys->mem.disk.name));
    memcpy(sys->mem.disk.name, game->content.name, 
      min(sizeof(sys->mem.disk.name), sizeof(game->content.name)));
    memcpy(sys->mem.disk.buffer, game, sizeof(Game));
    sys->mem.disk.code = DISK_CODE_WRITE;
    _interrupt(sys, INTERRUPT_CODE_DISK);

    system_PopProc(sys);
    editProcess_Execute(sys);
  }
}

static void createProcess_Execute(System *sys)
{
  CreateProcess *sysProc = createProcess_Create();
  system_PushProc(sys, sysProc, createProcess_Tick, NULL, createProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif