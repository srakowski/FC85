#ifndef FC85_PROC_IMPLEMENTATIONS
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
      ? 0
      : self->active;

    if (self->active < self->head)
      self->head = self->active;

    if (self->active - self->head >= 7)
      self->head++;
  }

  if (sys->mem.inpt.btns & INPT_BTN_UP)
  {
    self->active--;
    self->active = (sbyte)self->active < 0
      ? self->count - 1
      : self->active;

    while (self->active - self->head >= 7)
      self->head++;

    if ((sbyte)self->active < (sbyte)self->head)
      self->head = self->active;
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

static void menuTab_Draw(MenuTab *self, System *sys)
{
  for (int m = 0, i = self->head; i < self->count; m++, i++) 
  {
    char name[17] = {'\0'};
    sprintf(name, "%d:", (i + 1) < 10 ? (i + 1) : 0);
    _output(sys, m+1, 0, name, 
      i == self->active ? DISP_FLAG_INVERT : DISP_FLAG_NONE);
    
    strncpy(name, self->items[i].name, sizeof(name) - 3);
    _output(sys, m+1, 2, name, DISP_FLAG_NONE);
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
      : self->active;
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

static void menuProcess_Draw(MenuProcess *self, System *sys)
{
  _clrHome(sys);

  if (self->count > 1) 
  {
    for (byte c = self->head, i = 0; c < self->count; c++, i++) 
    {
      bool isActiveTab = c == self->active;
      MenuTab *tab = (MenuTab *)&self->tabs[c];
      char tabNameBuffer[5] = {'\0'};
      strncpy(tabNameBuffer, tab->name, sizeof(tabNameBuffer) - 1);
      _output(sys, 0, (i * 5) + (self->head > 0 ? 1 : 0), 
        tabNameBuffer, isActiveTab ? DISP_FLAG_INVERT : DISP_FLAG_NONE);
      if (isActiveTab)
        menuTab_Draw(tab, sys);
    }

    if (self->count - self->head > 3) {
      _outputc(sys, 0, DISP_CHAR_CELL_COLS - 1, 240, DISP_FLAG_NONE);
    }

    if (self->head > 0) {
      _outputc(sys, 0, 0, 240, DISP_FLAG_NONE);
    }
  }
  else 
  {
    _output(sys, 0, 0, self->tabs[0].name, DISP_FLAG_INVERT);
    menuTab_Draw(&self->tabs[0], sys);
  }
}

static void menuProcess_Tick(MenuProcess *self, System *sys)
{
  menuProcess_HandleInput(self, sys);
  menuProcess_Draw(self, sys);
}

/* ------------------------------------------------------------------------- */
#endif
#endif