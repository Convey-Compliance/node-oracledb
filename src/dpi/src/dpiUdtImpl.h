#ifndef DPIUDTIMPL_ORACLE
# define DPIUDTIMPL_ORACLE

#include <dpiEnv.h>
#include <dpiUdt.h>

namespace dpi
{

class UdtImpl : public Udt
{
public:
  OCIType *objType_;
  unsigned int outFormat_;

  UdtImpl (OCIEnv *envh, OCISvcCtx *svch, OCIType *objType);
  virtual v8::Local<v8::Object> toJsObject(void *obj_buf, unsigned int outFormat);
  static double ocidateToMsecSinceEpoch(const OCIDate *date);
private:
  OCIEnv   *envh_;
  OCISvcCtx *svch_;
  OCIError *errh_;
  v8::Local<v8::Value> UdtImpl::primitiveToJsObj(OCITypeCode typecode, void *attr_value);
  v8::Local<v8::Object> toJsObject(OCIType *tdo, void *obj_buf, void *obj_null);
};

};

#endif
