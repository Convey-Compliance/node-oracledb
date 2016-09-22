#ifndef DPIUDTIMPL_ORACLE
# define DPIUDTIMPL_ORACLE

#include <dpiEnv.h>
#include <dpiUdt.h>
#include <string>

namespace dpi
{

class UdtImpl : public Udt
{
public:
  UdtImpl (OCIEnv *envh, OCISvcCtx *svch, const std::string &objTypeName);
  virtual v8::Local<v8::Value> ociToJs(void *ind, void *obj_buf, unsigned int outFormat);
  virtual void * jsToOci(v8::Local<v8::Object> jsObj) override;
  static double ocidateToMsecSinceEpoch(const OCIDate *date);
  static OCIDate msecSinceEpochToOciDate(double msec);
  const OCIType * getType() const;
private:
  OCIEnv   *envh_;
  OCISvcCtx *svch_;
  OCIError *errh_;
  OCIType *objType_;
  unsigned int outFormat_;
  OCINumber _num;
  OCIDate _date;
  v8::Local<v8::Value> UdtImpl::ociValToJsVal(OCIInd ind, OCITypeCode typecode, void *attr_value);
  v8::Local<v8::Value> ociToJs(OCIType *tdo, void *obj_buf, void *obj_null);
  void jsArrToOciNestedTable(v8::Local<v8::Object> jsObj, void *ociObj, void *collHandle);
  void jsObjToOciUdt(v8::Local<v8::Object> jsObj, void *ociObj, void *ociObjHandle, OCIType *ociObjTdo);
  void * jsToOci(OCIType *tdo, v8::Local<v8::Object> jsObj);
  void * jsValToOciVal(v8::Local<v8::Value> jsVal, void *ociValHandle);
};

};

#endif
