#ifndef _proc_sys_h_
#define _proc_sys_h_
/* ------------------------------------------------------------------------- */

typedef struct {
  MenuProcess base;
} SysProcess;

static void sysProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_sys_c_
#define _proc_sys_c_
/* ------------------------------------------------------------------------- */

static SysProcess *sysProcess_Create()
{
  SysProcess *self = (SysProcess *)calloc(1, sizeof(SysProcess));
  assert(self);
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
  system_PushProc(sys, sysProc, sysProcess_Tick, sysProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif