#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_games_h_
#define _proc_games_h_
/* ------------------------------------------------------------------------- */

#include "proc_create.h"
#include "proc_edit.h"

typedef struct {
  MenuProcess base;
} GamesProcess;

static void gamesProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_games_c_
#define _proc_games_c_
/* ------------------------------------------------------------------------- */

static void gamesProcess_LoadGameFile(MenuItem *self, System *sys)
{
  strncpy(sys->mem.disk.name, self->name, 
    min(sizeof(sys->mem.disk.name), sizeof(self->name)) - 1);
  sys->mem.disk.code = DISK_CODE_READ;
  _interrupt(sys, INTERRUPT_CODE_DISK);
  memset(sys->mem.appl, 0, sizeof(sys->mem.appl));
  memcpy(sys->mem.appl, sys->mem.disk.buffer, 
    min(sizeof(sys->mem.appl), sizeof(sys->mem.disk.buffer)));
}

static void gamesProcess_menuItem_PlayExecute(MenuItem *self, System *sys)
{
  gamesProcess_LoadGameFile(self, sys);
}

static void gamesProcess_menuItem_EditExecute(MenuItem *self, System *sys)
{
  gamesProcess_LoadGameFile(self, sys);
  editProcess_Execute(sys);
}

static void gamesProcess_menuItem_CreateGameExecute(MenuItem *self, System *sys)
{
  createProcess_Execute(sys);
}

static void gameProcess_ReloadMenu(GamesProcess *self, System *sys)
{
  memset(&self->base, 0, sizeof(self->base));

  MenuTab tab;
  MenuItem item;

  sys->mem.disk.code = DISK_CODE_DIR;
  _interrupt(sys, INTERRUPT_CODE_DISK);
  struct _file **dir = (struct _file **)sys->mem.disk.buffer;

  // PLAY Tab

  memset(&tab, 0, sizeof(tab));
  strncpy(tab.name, "PLAY", sizeof(tab.name) - 1);
  for (int i = 0; dir[i] != NULL; i++)
  {
    memset(&item, 0, sizeof(item));
    strncpy(item.name, dir[i]->name, min(sizeof(item.name) - 1, sizeof(dir[i]->name) - 1));
    item.execute = gamesProcess_menuItem_PlayExecute;
    menuTab_AddItem(&tab, &item);      
  }
  menuProcess_AddTab(&self->base, &tab);

  // EDIT Tab

  memset(&tab, 0, sizeof(tab));
  strncpy(tab.name, "EDIT", sizeof(tab.name) - 1);
  for (int i = 0; dir[i] != NULL; i++)
  {
    memset(&item, 0, sizeof(item));
    strncpy(item.name, dir[i]->name, min(sizeof(item.name) - 1, sizeof(dir[i]->name) - 1));
    item.execute = gamesProcess_menuItem_EditExecute;
    menuTab_AddItem(&tab, &item);
  }
  menuProcess_AddTab(&self->base, &tab);

  // NEW Tab

  memset(&tab, 0, sizeof(tab));
  strncpy(tab.name, "NEW", sizeof(tab.name) - 1);
  
  memset(&item, 0, sizeof(item));
  strncpy(item.name, "Create Game", sizeof(item.name) - 1);
  item.execute = gamesProcess_menuItem_CreateGameExecute;
  menuTab_AddItem(&tab, &item);
  
  menuProcess_AddTab(&self->base, &tab);
}

static GamesProcess *gamesProcess_Create(System *sys)
{
  GamesProcess *self = (GamesProcess *)calloc(1, sizeof(GamesProcess));
  assert(self);
  gameProcess_ReloadMenu(self, sys);
  return self;
}

static void gamesProcess_Destroy(GamesProcess *self)
{
  assert(self);
  memset(self, 0, sizeof(GamesProcess));
  free(self);
}

static void gamesProcess_Tick(GamesProcess *self, System *sys)
{
  menuProcess_Tick(&self->base, sys);
}

static void gamesProcess_Execute(System *sys)
{
  GamesProcess *sysProc = gamesProcess_Create(sys);
  system_PushProc(sys, sysProc, gamesProcess_Tick, gameProcess_ReloadMenu, gamesProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif