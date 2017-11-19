#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_edit_h_
#define _proc_edit_h_
/* ------------------------------------------------------------------------- */

#include "proc_code.h"

typedef struct {
  MenuProcess base;
} EditProcess;

static void editProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_edit_c_
#define _proc_edit_c_
/* ------------------------------------------------------------------------- */

static void editProcess_menuItem_CodeExecute(MenuItem *self, System *sys)
{
  codeProcess_Execute(sys);
}

static EditProcess *editProcess_Create(System *sys)
{
  EditProcess *self = (EditProcess *)calloc(1, sizeof(EditProcess));
  assert(self);

  Game *game = (Game *)sys->mem.appl;

  MenuTab tab;
  MenuItem item;

  memset(&tab, 0, sizeof(tab));
  strncpy(tab.name, "GAME:", sizeof(tab.name) - 1);
  strncat(tab.name, game->content.name, sizeof(tab.name) - strlen(tab.name) - 1);

  memset(&item, 0, sizeof(item));
  strncpy(item.name, "Edit Code", sizeof(item.name) - 1);
  item.execute = editProcess_menuItem_CodeExecute;
  menuTab_AddItem(&tab, &item);

  memset(&item, 0, sizeof(item));
  strncpy(item.name, "Play", sizeof(item.name) - 1);
  menuTab_AddItem(&tab, &item);

  menuProcess_AddTab(&self->base, &tab);
  return self;
}

static void editProcess_Destroy(EditProcess *self)
{
  assert(self);
  memset(self, 0, sizeof(EditProcess));
  free(self);
}

static void editProcess_Tick(EditProcess *self, System *sys)
{
  menuProcess_Tick(&self->base, sys);
}

static void editProcess_Execute(System *sys)
{
  EditProcess *proc = editProcess_Create(sys);
  system_PushProc(sys, proc, editProcess_Tick, NULL, editProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif