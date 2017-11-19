#ifndef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_code_h_
#define _proc_code_h_
/* ------------------------------------------------------------------------- */

typedef struct {
  dword head;
  byte row;
  byte col;
  byte on;
  float timer;
} CodeProcess;

static void codeProcess_Execute(System *sys);

/* ------------------------------------------------------------------------- */
#endif
#endif
#ifdef FC85_PROC_IMPLEMENTATIONS
#ifndef _proc_code_c_
#define _proc_code_c_
/* ------------------------------------------------------------------------- */

static CodeProcess *codeProcess_Create()
{
  CodeProcess *self = (CodeProcess *)calloc(1, sizeof(CodeProcess));
  assert(self);
  return self;
}

static void codeProcess_Destroy(CodeProcess *self)
{
  assert(self);
  memset(self, 0, sizeof(CodeProcess));
  free(self);
}

static void codeProcess_Tick(CodeProcess *self, System *sys)
{
  _clrHome(sys);

  self->timer += sys->mem.sys.delta;
  if (self->timer > 0.5f) 
  {
    self->on = !self->on;
    self->timer = 0.0f;
  }
  _outputc(sys, self->row, self->col,
    219, self->on  ? DISP_FLAG_NONE : DISP_FLAG_INVERT);
}

static void codeProcess_Execute(System *sys)
{
  CodeProcess *proc = codeProcess_Create();
  system_PushProc(sys, proc, codeProcess_Tick, NULL, codeProcess_Destroy);
}

/* ------------------------------------------------------------------------- */
#endif
#endif