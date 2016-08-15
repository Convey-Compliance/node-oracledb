#ifndef DPIUDT_ORACLE
# define DPIUDT_ORACLE

#include <v8.h>

namespace dpi
{

class Udt
{
public:
  virtual ~Udt() {};
  virtual v8::Local<v8::Object> toJsObject(void *obj_buf, unsigned int outFormat) = 0;
};

}

#endif
