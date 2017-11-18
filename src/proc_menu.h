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
  byte itemsCnt;
  MenuItem items[MENU_MAX_MENU_ITEMS];
} MenuTab;

typedef struct {
  byte tabsCnt;
  MenuTab tabs[MENU_MAX_TABS];
} MenuProcess;

/* ------------------------------------------------------------------------- */
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_menu_c_
#define _proc_menu_c_
/* ------------------------------------------------------------------------- */

static void menuProcess_Tick(MenuProcess *self, System *sys)
{
  
}

/* ------------------------------------------------------------------------- */
#endif
#endif