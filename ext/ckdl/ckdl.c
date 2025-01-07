#include "ckdl.h"

VALUE rb_mKDL;

RUBY_FUNC_EXPORTED void
Init_ckdl(void)
{
  rb_mKDL = rb_define_module("KDL");
}
