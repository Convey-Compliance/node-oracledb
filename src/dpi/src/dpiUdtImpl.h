#ifndef DPIUDTIMPL_ORACLE
#define DPIUDTIMPL_ORACLE

#include <dpiEnv.h>
#include <dpiUdt.h>
#include <string>
#include "dpiExceptionImpl.h"

namespace dpi
{

class UdtException : public ExceptionImpl {
public:
  UdtException(const char *message) : ExceptionImpl("", 0, message) {}
};

class UdtImpl : public Udt
{
public:
  UdtImpl (OCIEnv *envh, OCISvcCtx *svch, OCIError *errh, const std::string &objTypeName);

  virtual v8::Local<v8::Value> ociToJs(void *ociVal, void *ociValNullStruct, unsigned int outFormat);
  virtual void * jsToOci(v8::Local<v8::Object> jsObj) override;

  const OCIType * getType() const;
private:
  OCIEnv    *envh_;
  OCISvcCtx *svch_;
  OCIError  *errh_;
  OCIType   *objType_;
  unsigned int outFormat_;
  OCINumber    _num;
  OCIDate      _date;

  v8::Local<v8::Value> ociToJs(void *ociVal, OCIType *ociValTdo, OCIInd *ociValNullStruct) const;
  v8::Local<v8::Object> ociObjToJsObj(void *ociObj, void *ociObjHandle, OCIType *ociObjTdo, OCIInd *ociObjNullStruct) const;
  v8::Local<v8::Array> ociNestedTableToJsArr(OCIColl *ociTab, void *ociTabHandle) const;
  v8::Local<v8::Value> UdtImpl::ociPrimitiveToJsPrimitive(void *ociPrimitive, OCIInd ociPrimitiveInd, OCITypeCode ociPrimitiveTypecode) const;

  void * jsToOci(v8::Local<v8::Value> jsVal, OCIType *ociValTdo);
  void jsObjToOciObj(v8::Local<v8::Object> jsObj, void *ociObj, void *ociObjHandle, OCIType *ociObjTdo);
  void jsArrToOciNestedTable(v8::Local<v8::Array> jsArr, OCIColl *ociTab, void *ociTabHandle);
  void * jsPrimitiveToOciPrimitive(v8::Local<v8::Value> jsPrimitive, OCITypeCode ociPrimitiveTypecode);

  static double ocidateToMsecSinceEpoch(const OCIDate *date);
  static OCIDate msecSinceEpochToOciDate(double msec);
  void getOciObjFields(void *ociObjHandle, ub2 &fieldsCount, void *&fieldsHandle) const;
  void getOciObjField(void *fieldsHandle, ub2 fieldIndex, const oratext *&fieldNamePtr, ub4 &fieldNameSize, OCITypeCode &fieldTypecode) const;
  void describeOciTdo(OCIType *tdo, OCIDescribe *&describeHandle, void *&paramHandle, OCITypeCode &typecode) const;
};

};

#endif
