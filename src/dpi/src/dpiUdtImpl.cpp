#include "dpiUdtImpl.h"
#include <dpiUtils.h>
#include "../../njs/src/njsUtils.h"
#include <ctime>
#include <map>
#include <sstream>

extern "C" {
#include "orid.h"
}
#include "nan.h"

using namespace dpi;

static string toLower(string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
}

static string toUpper(string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);
  return s;
}

UdtImpl::UdtImpl (OCIEnv *envh, OCISvcCtx *svch, OCIError *errh, const std::string &objTypeName)
  : envh_ (envh), svch_ (svch), errh_(errh)
{
  outFormat_ = 0;

  string upperObjTypeName = toUpper(objTypeName);
  ociCall (OCITypeByName (envh_, errh_, svch_, NULL, 0, (oratext*)upperObjTypeName.c_str(), (ub4)upperObjTypeName.size(),
                          NULL, 0, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &objType_), errh_);
}

const OCIType * UdtImpl::getType() const {
  return objType_;
}

double UdtImpl::ocidateToMsecSinceEpoch(const OCIDate *date) {
  struct tm future;

  future.tm_sec = date->OCIDateTime.OCITimeSS;
  future.tm_min = date->OCIDateTime.OCITimeMI;
  future.tm_hour = date->OCIDateTime.OCITimeHH;
  future.tm_mday = date->OCIDateDD;
  future.tm_mon = date->OCIDateMM - 1; // should start from 0
  future.tm_year = date->OCIDateYYYY - 1900;
  future.tm_isdst = -1;

  return (double)(mktime(&future) * 1000);
}

OCIDate UdtImpl::msecSinceEpochToOciDate(double msec) {
  std::time_t t = static_cast<time_t>(msec / 1000);

  struct tm* ltime = localtime(&t);
  sb2 year = ltime->tm_year + 1900;
  ub1 month = ltime->tm_mon + 1;

  OCIDate date;
  OCIDateSetDate(&date, year, month, ltime->tm_mday);
  OCIDateSetTime(&date, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

  return date;
}

v8::Local<v8::Value> UdtImpl::ociPrimitiveToJsPrimitive(void *ociPrimitive, OCIInd ociPrimitiveInd, OCITypeCode ociPrimitiveTypecode) const {
  if (ociPrimitiveInd == OCI_IND_NULL)
    return Nan::Null();

  switch (ociPrimitiveTypecode) {
    case OCI_TYPECODE_DATE :
      return Nan::New<v8::Date>(ocidateToMsecSinceEpoch((OCIDate *)ociPrimitive)).ToLocalChecked();
    case OCI_TYPECODE_RAW : {
      OCIRaw *rawPtr = *(OCIRaw**)ociPrimitive;
      auto raw = (char*)OCIRawPtr(envh_, rawPtr);
      auto rawSize = OCIRawSize(envh_, rawPtr);
      return Nan::CopyBuffer(raw, rawSize).ToLocalChecked();
    }
    case OCI_TYPECODE_CHAR :
    case OCI_TYPECODE_VARCHAR :
    case OCI_TYPECODE_VARCHAR2 : {
      OCIString *strPtr = *(OCIString**)ociPrimitive;
      auto str = (char*)OCIStringPtr(envh_, strPtr);
      auto strSize = OCIStringSize(envh_, strPtr);

      return Nan::New<v8::String>(str, strSize).ToLocalChecked();
    }
    case OCI_TYPECODE_UNSIGNED16 :
    case OCI_TYPECODE_UNSIGNED32 :
    case OCI_TYPECODE_REAL :
    case OCI_TYPECODE_DOUBLE :
    case OCI_TYPECODE_INTEGER :
    case OCI_TYPECODE_SIGNED16 :
    case OCI_TYPECODE_SIGNED32 :
    case OCI_TYPECODE_DECIMAL :
    case OCI_TYPECODE_FLOAT :
    case OCI_TYPECODE_NUMBER :
    case OCI_TYPECODE_SMALLINT : {
      double dnum;
      ociCall (OCINumberToReal(errh_, (OCINumber*)ociPrimitive, sizeof dnum, &dnum), errh_);

      return Nan::New<v8::Number>(dnum);
    }
    default: break;
  }

  return Nan::Null();
}

v8::Local<v8::Value> UdtImpl::ociToJs(void *ociVal, void *ociValNullStruct, unsigned int outFormat) {
  outFormat_ = outFormat;

  auto res =  ociToJs(ociVal, objType_, (OCIInd*)ociValNullStruct);
  if (ociVal)
    ociCall (OCIObjectFree(envh_, errh_, ociVal, 0), errh_);
  return res;
}

v8::Local<v8::Object> UdtImpl::ociObjToJsObj(void *ociObj, void *ociObjHandle, OCIType *ociObjTdo, OCIInd *ociObjNullStruct) const {
  ub2 fieldsCount;
  void *fieldsHandle;
  getOciObjFields(ociObjHandle, fieldsCount, fieldsHandle);

  v8::Local<v8::Object> jsObj;
  if (outFormat_ == NJS_ROWS_ARRAY)
    jsObj = Nan::New<v8::Array>(fieldsCount);
  else
    jsObj = Nan::New<v8::Object>();

  for (ub2 i = 1; i <= fieldsCount; i++) {
    const oratext *fieldNamePtr;
    ub4 fieldNameSize;
    OCITypeCode fieldTypecode;
    getOciObjField(fieldsHandle, i, fieldNamePtr, fieldNameSize, fieldTypecode);

    OCIType *fieldTdo = nullptr;
    OCIInd *fieldNullStruct = nullptr;
    void *fieldVal = nullptr;
    OCIInd fieldNullStatus = 0;
    ociCall (OCIObjectGetAttr (envh_, errh_, ociObj, ociObjNullStruct, ociObjTdo, &fieldNamePtr, &fieldNameSize, 1, 0, 0,
                               &fieldNullStatus, (void**)&fieldNullStruct, &fieldVal, &fieldTdo), errh_);

    v8::Local<v8::Value> val;
    switch (fieldTypecode) {
    case OCI_TYPECODE_NAMEDCOLLECTION:
    case OCI_TYPECODE_OBJECT:
      val = ociToJs(fieldVal, fieldTdo, fieldNullStruct);
      break;
    default:
      val = ociPrimitiveToJsPrimitive(fieldVal, fieldNullStatus, fieldTypecode);
    }

    if (jsObj->IsArray())
      Nan::Set(jsObj.As<v8::Array>(), i - 1, val);
    else {
      auto jsFieldName = Nan::New<v8::String>((char*)fieldNamePtr, fieldNameSize).ToLocalChecked();
      Nan::Set(jsObj.As<v8::Object>(), jsFieldName, val);
    }
  }

  return jsObj;
}

v8::Local<v8::Array> UdtImpl::ociNestedTableToJsArr(OCIColl *ociTab, void *ociTabHandle) const {
  void *collElemHandle = nullptr;
  ociCall (OCIAttrGet (ociTabHandle, OCI_DTYPE_PARAM, &collElemHandle, 0, OCI_ATTR_COLLECTION_ELEMENT, errh_), errh_);
  OCIRef *collElemTypeRef = nullptr;
  ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypeRef, 0, OCI_ATTR_REF_TDO, errh_), errh_);
  OCIType *collElemType;
  ociCall (OCITypeByRef (envh_, errh_, collElemTypeRef, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &collElemType), errh_);
  OCITypeCode collElemTypecode;
  ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

  sb4 collSize = 0;
  ociCall (OCICollSize (envh_, errh_, ociTab, &collSize), errh_);
  auto arr = Nan::New<v8::Array>(collSize);
  for (sb4 i = 0; i < collSize; i++) {
    sb4 exists;
    void *elem = nullptr;
    OCIInd *elemNull = nullptr;
    ociCall (OCICollGetElem (envh_, errh_, ociTab, i, &exists, &elem, (void**)&elemNull), errh_);
    if (!exists)
      continue;

    v8::Local<v8::Value> collElemVal;
    switch (collElemTypecode) {
    case OCI_TYPECODE_NAMEDCOLLECTION:
    case OCI_TYPECODE_OBJECT:
      collElemVal = ociToJs(elem, collElemType, elemNull);
      break;
    default:
      collElemVal = ociPrimitiveToJsPrimitive(elem, *(short*)elemNull, collElemTypecode);
    }

    Nan::Set(arr, i, collElemVal);
  }

  return arr;
}

v8::Local<v8::Value> UdtImpl::ociToJs(void *ociVal, OCIType *ociValTdo, OCIInd *ociValNullStruct) const {
  if (*ociValNullStruct == OCI_IND_NULL)
    return Nan::Null();

  OCIDescribe *describeHandle = nullptr;
  void *paramHandle = nullptr;
  OCITypeCode typecode = 0;
  describeOciTdo(ociValTdo, describeHandle, paramHandle, typecode);

  v8::Local<v8::Value> jsObj;
  switch (typecode) {
    case OCI_TYPECODE_OBJECT:
      jsObj = ociObjToJsObj(ociVal, paramHandle, ociValTdo, ociValNullStruct);
      break;
    case OCI_TYPECODE_TABLE:
      jsObj = ociNestedTableToJsArr((OCITable*)ociVal, paramHandle);
      break;
    default:
      jsObj = Nan::Null();
      break;
  }

  ociCall (OCIHandleFree (describeHandle, OCI_HTYPE_DESCRIBE), errh_);

  return jsObj;
}

void * UdtImpl::jsToOci(v8::Local<v8::Object> jsObj) {
  return jsToOci(jsObj, objType_);
}

void * UdtImpl::jsPrimitiveToOciPrimitive(v8::Local<v8::Value> jsPrimitive, OCITypeCode ociPrimitiveTypecode) {
  void *ociVal = nullptr;

  switch (ociPrimitiveTypecode) {
    case OCI_TYPECODE_DATE : {
      if (!jsPrimitive->IsDate())
        throw UdtException("date value required for UDT bind");

      double millisSinceEpoch = jsPrimitive.As<v8::Date>()->NumberValue();
      _date = msecSinceEpochToOciDate(millisSinceEpoch);
      ociVal = &_date;
      break;
    }
    case OCI_TYPECODE_RAW : {
      if (!jsPrimitive->IsObject() || !node::Buffer::HasInstance(jsPrimitive->ToObject()))
        throw UdtException("Buffer value required for UDT bind");

      auto jsObj = jsPrimitive->ToObject();
      size_t jsBufLen = node::Buffer::Length(jsObj);
      void *jsBuf = node::Buffer::Data(jsObj);

      auto raw = (OCIRaw **)&ociVal;
      ociCall (OCIRawResize(envh_, errh_, (ub4)jsBufLen, raw), errh_);
      ociCall (OCIRawAssignBytes(envh_, errh_, (ub1*)jsBuf, (ub4)jsBufLen, raw), errh_);
      break;
    }
    case OCI_TYPECODE_CHAR :
    case OCI_TYPECODE_VARCHAR :
    case OCI_TYPECODE_VARCHAR2 : {
      if (!jsPrimitive->IsString())
        throw UdtException("string value required for UDT bind");

      v8::String::Utf8Value jsStr(jsPrimitive->ToString());

      ociCall(OCIStringResize(envh_, errh_, jsStr.length(), (OCIString**)&ociVal), errh_);
      ociCall(OCIStringAssignText(envh_, errh_, (oratext*)*jsStr, jsStr.length(), (OCIString**)&ociVal), errh_);
      break;
    }
    case OCI_TYPECODE_UNSIGNED16 :
    case OCI_TYPECODE_UNSIGNED32 :
    case OCI_TYPECODE_REAL :
    case OCI_TYPECODE_DOUBLE :
    case OCI_TYPECODE_INTEGER :
    case OCI_TYPECODE_SIGNED16 :
    case OCI_TYPECODE_SIGNED32 :
    case OCI_TYPECODE_DECIMAL :
    case OCI_TYPECODE_FLOAT :
    case OCI_TYPECODE_NUMBER :
    case OCI_TYPECODE_SMALLINT : {
      if (!jsPrimitive->IsNumber())
        throw UdtException("number value required for UDT bind");

      double dblVal = jsPrimitive->ToNumber()->Value();
      ociCall(OCINumberFromReal(errh_, &dblVal, sizeof(double), &_num), errh_);
      ociVal = &_num;
      break;
    }
    default:
      throw UdtException("datatype is not supported for UDT bind");
  }

  return ociVal;
}

static map<string, string> getLowerCaseJsPropNames(v8::Local<v8::Object> jsObj) {
  map<string, string> names;
  v8::Local<v8::Array> jsObjPropNames = jsObj->GetOwnPropertyNames();

  for (uint32_t i = 0; i < jsObjPropNames->Length(); i++) {
    v8::String::Utf8Value caseSensitivePropName(jsObjPropNames->Get(i)->ToString());
    string lowerCasePropName = toLower(*caseSensitivePropName);
    if (names.count(lowerCasePropName) > 0) {
      stringstream err;
      err << "Js object contains duplicated case-insensitive fields " <<
        names[lowerCasePropName] << " and " << *caseSensitivePropName;
      throw UdtException(err.str().c_str());
    }

    names[lowerCasePropName] = *caseSensitivePropName;
  }

  return names;
}

void UdtImpl::jsArrToOciNestedTable(v8::Local<v8::Array> jsArr, OCIColl *ociTab, void *ociTabHandle) {
  void *collElemHandle = nullptr;
  ociCall (OCIAttrGet (ociTabHandle, OCI_DTYPE_PARAM, &collElemHandle, 0, OCI_ATTR_COLLECTION_ELEMENT, errh_), errh_);
  OCIRef *collElemTypeRef = nullptr;
  ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypeRef, 0, OCI_ATTR_REF_TDO, errh_), errh_);
  OCIType *collElemType;
  ociCall (OCITypeByRef (envh_, errh_, collElemTypeRef, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &collElemType), errh_);
  OCITypeCode collElemTypecode;
  ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

  for (uint32_t i = 0; i < jsArr->Length(); ++i) {
    v8::Local<v8::Value> jsVal = jsArr->Get(i);

    void *collElemVal;
    switch (collElemTypecode) {
    case OCI_TYPECODE_NAMEDCOLLECTION:
    case OCI_TYPECODE_OBJECT:
      collElemVal = jsToOci(jsVal, collElemType);
      break;
    default:
      collElemVal = jsPrimitiveToOciPrimitive(jsVal, collElemTypecode);
    }

    ociCall(OCICollAppend(envh_, errh_, collElemVal, 0, ociTab), errh_);
  }
}

void UdtImpl::getOciObjFields(void *ociObjHandle, ub2 &fieldsCount, void *&fieldsHandle) const {
  ociCall (OCIAttrGet (ociObjHandle, OCI_DTYPE_PARAM, &fieldsCount, 0, OCI_ATTR_NUM_TYPE_ATTRS, errh_), errh_);
  ociCall (OCIAttrGet (ociObjHandle, OCI_DTYPE_PARAM, &fieldsHandle, 0, OCI_ATTR_LIST_TYPE_ATTRS, errh_), errh_);
}

void UdtImpl::getOciObjField(void *fieldsHandle, ub2 fieldIndex, const oratext *&fieldNamePtr, ub4 &fieldNameSize, OCITypeCode &fieldTypecode) const {
  void *fieldHandle = nullptr;
  ociCall (OCIParamGet (fieldsHandle, OCI_DTYPE_PARAM, errh_, &fieldHandle, fieldIndex), errh_);
  ociCall (OCIAttrGet (fieldHandle, OCI_DTYPE_PARAM, &fieldNamePtr, &fieldNameSize, OCI_ATTR_NAME, errh_), errh_);
  ociCall (OCIAttrGet (fieldHandle, OCI_DTYPE_PARAM, &fieldTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);
}

void UdtImpl::jsObjToOciObj(v8::Local<v8::Object> jsObj, void *ociObj, void *ociObjHandle, OCIType *ociObjTdo) {
  ub2 fieldsCount;
  void *fieldsHandle;
  getOciObjFields(ociObjHandle, fieldsCount, fieldsHandle);
  auto lowerCaseJsPropNames = getLowerCaseJsPropNames(jsObj);

  for (ub2 i = 1; i <= fieldsCount; i++) {
    const oratext *fieldNamePtr;
    ub4 fieldNameSize;
    OCITypeCode fieldTypecode;
    getOciObjField(fieldsHandle, i, fieldNamePtr, fieldNameSize, fieldTypecode);

    string lowerCaseOciFieldName(toLower(string((char*)fieldNamePtr, fieldNameSize)));

    if (lowerCaseJsPropNames.count(lowerCaseOciFieldName) == 0)
      continue;

    string jsFieldName = lowerCaseJsPropNames[lowerCaseOciFieldName];
    v8::Local<v8::Value> jsField = jsObj->Get(Nan::New<v8::String>(jsFieldName).ToLocalChecked());

    if (jsField->IsNull())
      continue; // TODO: add test

    void *fieldValue;
    switch (fieldTypecode) {
    case OCI_TYPECODE_NAMEDCOLLECTION:
    case OCI_TYPECODE_OBJECT: {
      OCIType *fieldTdo = nullptr;
      ociCall (OCIObjectGetAttr (envh_, errh_, ociObj, nullptr, ociObjTdo, &fieldNamePtr, &fieldNameSize, 1, 0, 0,
                                  0, 0, 0, &fieldTdo), errh_);
      fieldValue = jsToOci(jsField, fieldTdo);
      break;
    }
    default:
      fieldValue = jsPrimitiveToOciPrimitive(jsField, fieldTypecode);
    }

    ociCall (OCIObjectSetAttr (envh_, errh_, ociObj, 0, ociObjTdo, &fieldNamePtr, &fieldNameSize, 1, 0, 0,
                               0, 0, fieldValue), errh_);
  }
}

void UdtImpl::describeOciTdo(OCIType *tdo, OCIDescribe *&describeHandle, void *&paramHandle, OCITypeCode &typecode) const {
  ociCall(OCIHandleAlloc(envh_, (void**)&describeHandle, OCI_HTYPE_DESCRIBE, 0, 0), errh_);
  ociCall(OCIDescribeAny(svch_, errh_, tdo, 0, OCI_OTYPE_PTR, 1, OCI_PTYPE_TYPE, describeHandle), errh_);
  ociCall(OCIAttrGet(describeHandle, OCI_HTYPE_DESCRIBE, &paramHandle, 0, OCI_ATTR_PARAM, errh_), errh_);

  ociCall(OCIAttrGet(paramHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);
  if (typecode == OCI_TYPECODE_NAMEDCOLLECTION)
    ociCall(OCIAttrGet(paramHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_COLLECTION_TYPECODE, errh_), errh_);
}

void * UdtImpl::jsToOci(v8::Local<v8::Value> jsVal, OCIType *ociValTdo) {
  if (!jsVal->IsArray() && !jsVal->IsObject())
    throw UdtException("only js array or object allowed for UDT binds");

  void *paramHandle = nullptr;
  OCIDescribe *describeHandle = nullptr;
  OCITypeCode typecode = 0;

  describeOciTdo(ociValTdo, describeHandle, paramHandle, typecode);

  if (jsVal->IsArray() && typecode != OCI_TYPECODE_TABLE)
    throw UdtException("js array binding possible only to oracle nested table datatype");
  if (jsVal->IsObject() && !jsVal->IsArray() && typecode != OCI_TYPECODE_OBJECT)
    throw UdtException("js object binding possible only to oracle user defined datatype");

  void *ociObj = nullptr;
  ociCall(OCIObjectNew(envh_, errh_, svch_, typecode, ociValTdo, 0, OCI_DURATION_STATEMENT, TRUE, &ociObj), errh_);

  if (jsVal->IsArray())
    jsArrToOciNestedTable(v8::Local<v8::Array>::Cast(jsVal), (OCIColl*)ociObj, paramHandle);
  else if (jsVal->IsObject())
    jsObjToOciObj(jsVal->ToObject(), ociObj, paramHandle, ociValTdo);

  if (describeHandle)
    ociCall (OCIHandleFree (describeHandle, OCI_HTYPE_DESCRIBE), errh_);

  return ociObj;
}
