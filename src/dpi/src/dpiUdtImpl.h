#ifndef DPIUDTIMPL_ORACLE
# define DPIUDTIMPL_ORACLE

#include <dpiEnv.h>
#include <dpiUdt.h>

namespace dpi
{

class UdtImpl : public Udt
{
public:
  OCIType *objType;
  OCITypeCode typecode;
  OCIDescribe *describeHandle;
  void *paramHandle;

  UdtImpl (void *stmtDesc, OCIEnv *envh, OCISvcCtx *svch);
  virtual v8::Local<v8::Object> toJsObject(void *obj_buf);
private:
  OCIEnv   *envh_;
  OCISvcCtx *svch_;
  OCIError *errh_;
  void *stmtDesc_;
  v8::Local<v8::Object> toJsObject(OCIType *tdo, void *obj_buf, void *obj_null);
};

};

#endif
