#ifndef DPIUDT_ORACLE
# define DPIUDT_ORACLE

#include <node.h>
#include <v8.h>

namespace dpi
{

class Udt
{
public:
  virtual ~Udt() {};
  virtual v8::Local<v8::Value> ociToJs(void *ociVal, void *ociValNullStruct, unsigned int outFormat) = 0;
  virtual void * jsToOci(v8::Local<v8::Object> jsObj) = 0;
};

}

#endif
