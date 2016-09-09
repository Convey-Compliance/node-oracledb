#ifndef DPIUDTIMPL_ORACLE
# define DPIUDTIMPL_ORACLE

#include <dpiEnv.h>
#include <dpiUdt.h>

namespace dpi
{

class UdtImpl : public Udt
{
public:
  UdtImpl (OCIEnv *envh, OCISvcCtx *svch, OCIType *objType);
  static double ocidateToMsecSinceEpoch(const OCIDate *date);

  virtual v8::Local<v8::Value> toJsObject(void *ind, void *obj_buf, unsigned int outFormat);
private:
  OCIEnv   *envh_;
  OCISvcCtx *svch_;
  OCIError *errh_;
  unsigned int outFormat_;
  OCIType *objType_;
  v8::Local<v8::Value> UdtImpl::primitiveToJsObj(OCIInd ind,OCITypeCode typecode, void *attr_value);
  v8::Local<v8::Value> toJsObject(OCIType *tdo, void *obj_buf, void *obj_null);
};

};

#endif
