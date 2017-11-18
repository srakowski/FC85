#ifndef _proc_menu_h_
#define _proc_menu_h_
/* ------------------------------------------------------------------------- */

#define MENU_MAX_TABS           32
#define MENU_TAB_NAME_SIZE      16
#define MENU_MAX_MENU_ITEMS     32
#define MENU_ITEM_NAME_SIZE     16
#define MENU_ITEM_TAG_SIZE      16

typedef struct {
  byte name[MENU_ITEM_NAME_SIZE];
  byte tag[MENU_ITEM_TAG_SIZE];
  void (*execute)(void *, void *);
} MenuItem;

typedef struct {
  byte name[MENU_TAB_NAME_SIZE];
  byte head;
  byte active;
  byte count;
  MenuItem items[MENU_MAX_MENU_ITEMS];
} MenuTab;

typedef struct {
  byte head;
  byte active;
  byte count;
  MenuTab tabs[MENU_MAX_TABS];
} MenuProcess;

/* ------------------------------------------------------------------------- */
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_menu_c_
#define _proc_menu_c_
/* ------------------------------------------------------------------------- */

static void menuTab_AddItem(MenuTab *self, MenuItem *item)
{
  assert(self && item);
  assert(self->count < MENU_MAX_MENU_ITEMS);
  memcpy(&self->items[self->count], item, sizeof(MenuItem));
  self->count++;
}

static void menuTab_HandleInput(MenuTab *self, System *sys)
{
  assert(self && sys);

  if (sys->mem.inpt.btns & INPT_BTN_DOWN)
  {
    self->active++;
    self->active = self->active >= self->count 
      ? self->count - 1
      : self->count;
    if (self->active - self->head >= 3)
      self->head++;
  }

  if (sys->mem.inpt.btns & INPT_BTN_UP)
  {
    self->active--;
    self->active = (sbyte)self->active < 0
      ? 0
      : self->active;
    if (self->active < self->head)
      self->head--;
  }

  if (sys->mem.inpt.btns & INPT_BTN_A)
  {
    if (self->items[self->active].execute)
    {
      self->items[self->active].execute(
        &self->items[self->active],
        sys
      );
    }
  }
}

static void menuProcess_AddTab(MenuProcess *self, MenuTab *tab)
{
  assert(self && tab);
  assert(self->count < MENU_MAX_TABS);
  memcpy(&self->tabs[self->count], tab, sizeof(MenuTab));
  self->count++;
}

static void menuProcess_HandleInput(MenuProcess *self, System *sys)
{
  assert(self && sys);

  if (sys->mem.inpt.btns & INPT_BTN_RIGHT)
  {
    self->active++;
    self->active = self->active >= self->count 
      ? self->count - 1
      : self->count;
    if (self->active - self->head >= 3)
      self->head++;
  }

  if (sys->mem.inpt.btns & INPT_BTN_LEFT)
  {
    self->active--;
    self->active = (sbyte)self->active < 0
      ? 0
      : self->active;
    if (self->active < self->head)
      self->head--;
  }

  menuTab_HandleInput(&self->tabs[self->active], sys);
}

static void menuProcess_Tick(MenuProcess *self, System *sys)
{
  menuProcess_HandleInput(self, sys);
}

/* ------------------------------------------------------------------------- */
#endif
#endif