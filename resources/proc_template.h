#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_games_h_
#define _proc_games_h_
/* ------------------------------------------------------------------------- */

typedef struct {
  bool pass;
} GamesProcess;

static void gamesProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_games_c_
#define _proc_games_c_
/* ------------------------------------------------------------------------- */

static GamesProcess *gamesProcess_Create()
{
  GamesProcess *self = (GamesProcess *)calloc(1, sizeof(GamesProcess));
  assert(self);
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
}

static void gamesProcess_Execute(System *sys)
{
  GamesProcess *sysProc = gamesProcess_Create();
  system_PushProc(sys, sysProc, gamesProcess_Tick, gamesProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif