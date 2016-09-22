#include "dpiUdtImpl.h"
#include <dpiUtils.h>
#include "../../njs/src/njsUtils.h"
#include <ctime>
#include <map>
#include <dpiExceptionImpl.h>
#include <sstream>

extern "C" {
#include "orid.h"
}
#include "nan.h"


using namespace dpi;

UdtImpl::UdtImpl (OCIEnv *envh, OCISvcCtx *svch, const std::string &objTypeName)
  : envh_ (envh), svch_ (svch)
{
  outFormat_ = 0;
  ociCallEnv (OCIHandleAlloc (envh, (void**)&errh_, OCI_HTYPE_ERROR, 0, 0), envh);

  ociCall (OCITypeByName (envh_, errh_, svch_, NULL, 0, (oratext*)objTypeName.c_str(), (ub4)objTypeName.size(), NULL, 0,
                          OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &objType_), errh_);
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

v8::Local<v8::Value> UdtImpl::ociValToJsVal(OCIInd ind, OCITypeCode typecode, void *attr_value) {
  if (ind == OCI_IND_NULL)
    return Nan::Null();

  switch (typecode) {
    case OCI_TYPECODE_DATE :
      return Nan::New<v8::Date>(ocidateToMsecSinceEpoch((OCIDate *)attr_value)).ToLocalChecked();
    case OCI_TYPECODE_RAW : {
      OCIRaw *rawPtr = *(OCIRaw**)attr_value;
      auto raw = (char*)OCIRawPtr(envh_, rawPtr);
      auto rawSize = OCIRawSize(envh_, rawPtr);
      return Nan::CopyBuffer(raw, rawSize).ToLocalChecked();
    }
    case OCI_TYPECODE_CHAR :
    case OCI_TYPECODE_VARCHAR :
    case OCI_TYPECODE_VARCHAR2 : {
      OCIString *strPtr = *(OCIString**)attr_value;
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
      ociCall (OCINumberToReal(errh_, (OCINumber*)attr_value, sizeof dnum, &dnum), errh_);

      return Nan::New<v8::Number>(dnum);
    }
  }
  return Nan::Null();
}

v8::Local<v8::Value> UdtImpl::ociToJs(void *ind, void *obj_buf, unsigned int outFormat) {
  outFormat_ = outFormat;
  auto oracleObj = *(void**)obj_buf;

  auto res =  ociToJs(objType_, oracleObj, ind);
  if (oracleObj)
    ociCall (OCIObjectFree(envh_, errh_, oracleObj, 0), errh_);
  return res;
}

v8::Local<v8::Value> UdtImpl::ociToJs(OCIType *tdo, void *obj_buf, void *obj_null) {
  v8::Local<v8::Value> obj;

  if (*(OCIInd*)obj_null == OCI_IND_NULL)
    return Nan::Null();

  OCIDescribe *describeHandle = 0;
  ociCall (OCIHandleAlloc (envh_, (dvoid **)&describeHandle, OCI_HTYPE_DESCRIBE, 0, 0), errh_);
  ociCall (OCIDescribeAny (svch_, (OCIError*)errh_, tdo, 0, OCI_OTYPE_PTR, 1, OCI_PTYPE_TYPE, describeHandle), errh_);
  dvoid *paramHandle = nullptr;
  ociCall (OCIAttrGet (describeHandle, OCI_HTYPE_DESCRIBE, &paramHandle, 0, OCI_ATTR_PARAM, errh_), errh_);

  OCITypeCode typecode = 0;
  ociCall (OCIAttrGet (paramHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

  switch (typecode) {
    case OCI_TYPECODE_OBJECT: {
      ub2 count;
      ociCall (OCIAttrGet (paramHandle, OCI_DTYPE_PARAM, &count, 0, OCI_ATTR_NUM_TYPE_ATTRS, errh_), errh_);
      dvoid *list_attr;
      ociCall (OCIAttrGet (paramHandle, OCI_DTYPE_PARAM, &list_attr, 0, OCI_ATTR_LIST_TYPE_ATTRS, errh_), errh_);

      uint32_t objArrIdx = 0;
      if (outFormat_ == NJS_ROWS_ARRAY)
        obj = Nan::New<v8::Array>(count);
      else
        obj = Nan::New<v8::Object>();

      for (ub2 j = 1; j <= count; j++) {
        v8::Local<v8::Value> val;
        dvoid *elemHandle = nullptr;
        ociCall (OCIParamGet (list_attr, OCI_DTYPE_PARAM, errh_, &elemHandle, j), errh_);
        char *elemName;
        ub4 elemNameSize;
        ociCall (OCIAttrGet (elemHandle, OCI_DTYPE_PARAM, &elemName, &elemNameSize, OCI_ATTR_NAME, errh_), errh_);
        auto key = Nan::New<v8::String>(elemName, elemNameSize).ToLocalChecked();

        OCIType *attr_tdo = nullptr;
        dvoid *attr_null_struct = nullptr;
        dvoid *attr_value = nullptr;
        OCIInd attr_null_status = 0;

        ociCall (OCIObjectGetAttr (envh_, errh_, obj_buf, obj_null, tdo, (const oratext**)&elemName, &elemNameSize, 1, 0, 0,
                                   &attr_null_status, &attr_null_struct, &attr_value, &attr_tdo), errh_);

        OCITypeCode elemTypecode;
        ociCall (OCIAttrGet (elemHandle, OCI_DTYPE_PARAM, &elemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

        switch (elemTypecode) {
        case OCI_TYPECODE_NAMEDCOLLECTION:
        case OCI_TYPECODE_OBJECT:
          val = ociToJs(attr_tdo, attr_value, attr_null_struct);
          break;
        default:
          val = ociValToJsVal(attr_null_status, elemTypecode, attr_value);
        }
        if (obj->IsArray())
          Nan::Set(obj.As<v8::Array>(), objArrIdx++, val);
        else
          Nan::Set(obj.As<v8::Object>(), key, val);
      }
      break;
    }
    case OCI_TYPECODE_NAMEDCOLLECTION: {
      OCITypeCode collTypecode = 0;
      ociCall (OCIAttrGet (paramHandle, OCI_DTYPE_PARAM, &collTypecode, 0, OCI_ATTR_COLLECTION_TYPECODE, errh_), errh_);
      switch (collTypecode) {
        case OCI_TYPECODE_TABLE: {
          void *collElemHandle = nullptr;
          ociCall (OCIAttrGet (paramHandle, OCI_DTYPE_PARAM, &collElemHandle, 0, OCI_ATTR_COLLECTION_ELEMENT, errh_), errh_);

          OCIRef *collElemRef = nullptr;
          ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemRef, 0, OCI_ATTR_REF_TDO, errh_), errh_);
          OCIType *collElemType;
          ociCall (OCITypeByRef (envh_, errh_, collElemRef, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &collElemType), errh_);

          sb4 collSize = 0;
          ociCall (OCICollSize (envh_, errh_, (OCIColl*)obj_buf, &collSize), errh_);
          auto arr = Nan::New<v8::Array>(collSize);
          for (sb4 i = 0; i < collSize; i++) {
            sb4 exists;
            void *elem = nullptr, *elemNull = nullptr;
            ociCall (OCICollGetElem (envh_, errh_, (OCIColl*)obj_buf, i, &exists, &elem, &elemNull), errh_);
            if (!exists)
              break;

            OCITypeCode collElemTypecode;
            ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);
            v8::Local<v8::Value> collElemVal;
            switch (collElemTypecode) {
            case OCI_TYPECODE_NAMEDCOLLECTION:
            case OCI_TYPECODE_OBJECT:
              collElemVal = ociToJs(collElemType, elem, elemNull);
              break;
            default:
              collElemVal = ociValToJsVal(*(short*)elemNull, collElemTypecode, elem);
            }

            Nan::Set(arr, i, collElemVal);
          }

          obj = arr;
        }
        break;
        default:
          break;
      }
    }
    break;
    default:
      break;
  }

  //ociCall (OCIHandleFree (describeHandle, OCI_HTYPE_DESCRIBE), errh_);

  return obj;
}

void * UdtImpl::jsToOci(v8::Local<v8::Object> jsObj) {
  return jsToOci(objType_, jsObj);
}

void * UdtImpl::jsValToOciVal(v8::Local<v8::Value> jsVal, void *ociValHandle) {
  void *ociVal = nullptr;
  OCITypeCode typecode = 0;
  ociCall (OCIAttrGet (ociValHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

  switch (typecode) {
    case OCI_TYPECODE_DATE : {
      if (!jsVal->IsDate())
        throw ExceptionImpl("", 0, "date value required for UDT bind");

      double millisSinceEpoch = jsVal.As<v8::Date>()->NumberValue();
      _date = msecSinceEpochToOciDate(millisSinceEpoch);
      ociVal = &_date;
      break;
    }
    case OCI_TYPECODE_RAW : {
      if (!node::Buffer::HasInstance(jsVal))
        throw ExceptionImpl("", 0, "Buffer value required for UDT bind");

      size_t jsBufLen = node::Buffer::Length(jsVal);
      void *jsBuf = node::Buffer::Data(jsVal);

      auto raw = (OCIRaw **)&ociVal;
      ociCall (OCIRawResize(envh_, errh_, (ub4)jsBufLen, raw), errh_);
      ociCall (OCIRawAssignBytes(envh_, errh_, (ub1*)jsBuf, (ub4)jsBufLen, raw), errh_);
      break;
    }
    case OCI_TYPECODE_CHAR :
    case OCI_TYPECODE_VARCHAR :
    case OCI_TYPECODE_VARCHAR2 : {
      if (!jsVal->IsString())
        throw ExceptionImpl("", 0, "string value required for UDT bind");

      v8::String::Utf8Value jsStr(jsVal->ToString());

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
      if (!jsVal->IsNumber())
        throw ExceptionImpl("", 0, "number value required for UDT bind");

      double dblVal = jsVal->ToNumber()->Value();
      ociCall(OCINumberFromReal(errh_, &dblVal, sizeof(double), &_num), errh_);
      ociVal = &_num;
      break;
    }
    case OCI_TYPECODE_OBJECT: {
      OCIRef *typeRef = nullptr;
      ociCall (OCIAttrGet (ociValHandle, OCI_DTYPE_PARAM, &typeRef, 0, OCI_ATTR_REF_TDO, errh_), errh_);
      OCIType *type;
      ociCall (OCITypeByRef (envh_, errh_, typeRef, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &type), errh_);
      ociVal = jsToOci(type, jsVal->ToObject());
      break;
    }
    default:
      throw ExceptionImpl("", 0, "datatype is not supported for UDT bind");
  }

  return ociVal;
}

static string toLower(string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
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
      throw ExceptionImpl("", 0, err.str().c_str());
    }

    names[lowerCasePropName] = *caseSensitivePropName;
  }

  return names;
}

void UdtImpl::jsArrToOciNestedTable(v8::Local<v8::Object> jsObj, void *ociObj, void *collHandle) {
  auto arr = v8::Local<v8::Array>::Cast(jsObj);
  for (uint32_t i = 0; i < arr->Length(); ++i) {
    v8::Local<v8::Value> jsVal = arr->Get(i);

    void *collElemHandle = nullptr;
    ociCall (OCIAttrGet (collHandle, OCI_DTYPE_PARAM, &collElemHandle, 0, OCI_ATTR_COLLECTION_ELEMENT, errh_), errh_);

    void *collVal = jsValToOciVal(jsVal, collElemHandle);
    ociCall(OCICollAppend(envh_, errh_, collVal, 0, (OCIColl *)ociObj), errh_);
  }
}

void UdtImpl::jsObjToOciUdt(v8::Local<v8::Object> jsObj, void *ociObj, void *ociObjHandle, OCIType *ociObjTdo) {
  ub2 fieldsCount;
  ociCall (OCIAttrGet (ociObjHandle, OCI_DTYPE_PARAM, &fieldsCount, 0, OCI_ATTR_NUM_TYPE_ATTRS, errh_), errh_);
  void *fieldsList;
  ociCall (OCIAttrGet (ociObjHandle, OCI_DTYPE_PARAM, &fieldsList, 0, OCI_ATTR_LIST_TYPE_ATTRS, errh_), errh_);
  auto lowerCaseJsPropNames = getLowerCaseJsPropNames(jsObj);

  for (ub2 j = 1; j <= fieldsCount; j++) {
    void *fieldHandle = nullptr;
    ociCall (OCIParamGet (fieldsList, OCI_DTYPE_PARAM, errh_, &fieldHandle, j), errh_);
    char *fieldNamePtr;
    ub4 fieldNameSize;
    ociCall (OCIAttrGet (fieldHandle, OCI_DTYPE_PARAM, &fieldNamePtr, &fieldNameSize, OCI_ATTR_NAME, errh_), errh_);
    string lowerCaseOciFieldName(toLower(string(fieldNamePtr, fieldNameSize)));

    void *fieldValue = nullptr;
    OCIInd fieldNullStatus = OCI_IND_NOTNULL;

    if (lowerCaseJsPropNames.count(lowerCaseOciFieldName) == 0)
      fieldNullStatus = OCI_IND_NULL;
    else {
      string jsFieldName = lowerCaseJsPropNames[lowerCaseOciFieldName];
      v8::Local<v8::Value> jsField = jsObj->Get(Nan::New<v8::String>(jsFieldName).ToLocalChecked());
      fieldValue = jsValToOciVal(jsField, fieldHandle);
    }

    void *fieldNullStruct = nullptr, *ociObjNullStruct = nullptr;
    ociCall (OCIObjectGetInd(envh_, errh_, ociObj, &ociObjNullStruct), errh_);
    ociCall (OCIObjectSetAttr (envh_, errh_, ociObj, ociObjNullStruct, ociObjTdo, (const oratext**)&fieldNamePtr, &fieldNameSize, 1, 0, 0,
                               fieldNullStatus, &fieldNullStruct, fieldValue), errh_);
  }
}

void * UdtImpl::jsToOci(OCIType *tdo, v8::Local<v8::Object> jsObj) {
  void *dschp = nullptr, *paramHandle = nullptr, *ociObj = nullptr;
  OCITypeCode typecode = 0;

  if (jsObj->IsArray() || jsObj->IsObject()) {
    ociCall(OCIHandleAlloc(envh_, &dschp, OCI_HTYPE_DESCRIBE, 0, 0), errh_);
    ociCall(OCIDescribeAny(svch_, errh_, tdo, 0, OCI_OTYPE_PTR, 1, OCI_PTYPE_TYPE, (OCIDescribe*)dschp), errh_);
    ociCall(OCIAttrGet(dschp, OCI_HTYPE_DESCRIBE, &paramHandle, 0, OCI_ATTR_PARAM, errh_), errh_);

    ociCall(OCIAttrGet(paramHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);
    if (typecode == OCI_TYPECODE_NAMEDCOLLECTION)
      ociCall(OCIAttrGet(paramHandle, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_COLLECTION_TYPECODE, errh_), errh_);

    ociCall(OCIObjectNew(envh_, errh_, svch_, typecode, tdo, 0, OCI_DURATION_SESSION, TRUE, &ociObj), errh_);
  }

  if (jsObj->IsArray()) {
    if (typecode != OCI_TYPECODE_TABLE)
      throw ExceptionImpl("", 0, "js array binding possible only to oracle nested table datatype");

    jsArrToOciNestedTable(jsObj, ociObj, paramHandle);
  } else if (jsObj->IsObject()) {
    if (typecode != OCI_TYPECODE_OBJECT)
      throw ExceptionImpl("", 0, "js object binding possible only to oracle user defined datatype");

    jsObjToOciUdt(jsObj, ociObj, paramHandle, tdo);
  } else
    throw ExceptionImpl("", 0, "only js array or object allowed for UDT binds");

  return ociObj;
}
