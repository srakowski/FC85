#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_sys_h_
#define _proc_sys_h_
/* ------------------------------------------------------------------------- */

#include "proc_games.h"

typedef struct {
  MenuProcess base;
} SysProcess;

static void sysProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_sys_c_
#define _proc_sys_c_
/* ------------------------------------------------------------------------- */

static void sysProcess_menuItem_GamesExecute(MenuItem *self, System *sys)
{
  gamesProcess_Execute(sys);
}

static void sysProcess_menuItem_ShutdownExecute(MenuItem *self, System *sys)
{
  system_SetShutdownFlag(sys);
}

static SysProcess *sysProcess_Create()
{
  SysProcess *self = (SysProcess *)calloc(1, sizeof(SysProcess));
  assert(self);

  MenuTab tab;
  MenuItem item;

  memset(&tab, 0, sizeof(tab));
  strncpy(tab.name, "FC-85:SYS", sizeof(tab.name) - 1);

  memset(&item, 0, sizeof(item));
  strncpy(item.name, "Games", sizeof(item.name) - 1);
  item.execute = sysProcess_menuItem_GamesExecute;
  menuTab_AddItem(&tab, &item);

  memset(&item, 0, sizeof(item));
  strncpy(item.name, "Shutdown", sizeof(item.name) - 1);
  item.execute = sysProcess_menuItem_ShutdownExecute;
  menuTab_AddItem(&tab, &item);

  menuProcess_AddTab(&self->base, &tab);
  return self;
}

static void sysProcess_Destroy(SysProcess *self)
{
  assert(self);
  memset(self, 0, sizeof(SysProcess));
  free(self);
}

static void sysProcess_Tick(SysProcess *self, System *sys)
{
  menuProcess_Tick(&self->base, sys);
}

static void sysProcess_Execute(System *sys)
{
  SysProcess *sysProc = sysProcess_Create();
  system_PushProc(sys, sysProc, sysProcess_Tick, NULL, sysProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif