#include "ckdl.h"

VALUE rb_mCkdl;

RUBY_FUNC_EXPORTED void
Init_ckdl(void)
{
  rb_mCkdl = rb_define_module("Ckdl");
}
