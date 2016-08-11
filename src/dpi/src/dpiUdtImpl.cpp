#include "dpiUdtImpl.h"
#include <dpiUtils.h>
extern "C" {
#include "orid.h"
}
#include "nan.h"

using namespace dpi;

UdtImpl::UdtImpl (void *stmtDesc, OCIEnv *envh, OCISvcCtx *svch)
  : stmtDesc_(stmtDesc), envh_ (envh), svch_ (svch)
{
  objType = nullptr;

  ociCallEnv (OCIHandleAlloc ((void *)envh, (void**)&errh_, OCI_HTYPE_ERROR, 0, (dvoid **)0), envh);

  text *defineName = NULL;
  ub4 defineNameSize = 0;
  ociCall (OCIAttrGet (stmtDesc_, OCI_DTYPE_PARAM, &defineName, &defineNameSize, OCI_ATTR_TYPE_NAME, errh_), errh_);

  ociCall (OCITypeByName (envh_, errh_, svch_, NULL, 0, defineName, defineNameSize, NULL, 0,
    OCI_DURATION_SESSION, OCI_TYPEGET_HEADER, &objType), errh_);
}

v8::Local<v8::Value> UdtImpl::primitiveToJsObj(OCITypeCode typecode, void *attr_value) {
  switch (typecode) {
    case OCI_TYPECODE_DATE :
      return Nan::New<v8::Date>(*(long double*)attr_value).ToLocalChecked();
    case OCI_TYPECODE_RAW : // TODO: support
      break;
    case OCI_TYPECODE_CHAR :
    case OCI_TYPECODE_VARCHAR :
    case OCI_TYPECODE_VARCHAR2 : {
      OCIString *vs = *(OCIString**)attr_value;
      auto str = (char*)OCIStringPtr(envh_,vs);
      auto strSize = OCIStringSize(envh_, vs);
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
    case OCI_TYPECODE_SMALLINT :
      return Nan::New<v8::Number>(*(double*)attr_value);
  }
  return Nan::Null();
}

v8::Local<v8::Object> UdtImpl::toJsObject(void *obj_buf) {
  auto oracleObj = *(void**)obj_buf;
  void *oracleObjNull = nullptr;
  ociCall (OCIObjectGetInd (envh_, errh_, oracleObj, &oracleObjNull), errh_);

  return toJsObject(objType, oracleObj, oracleObjNull);
}

v8::Local<v8::Object> UdtImpl::toJsObject(OCIType *tdo, void *obj_buf, void *obj_null) {
  auto obj = Nan::New<v8::Object>();

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
      for (ub2 j = 1; j <= count; j++) {
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

        if (attr_null_status != OCI_IND_NOTNULL) {
          Nan::Set(obj, key, Nan::Null());
          break;
        }

        OCITypeCode elemTypecode;
        ociCall (OCIAttrGet (elemHandle, OCI_DTYPE_PARAM, &elemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

        switch (elemTypecode) {
        case OCI_TYPECODE_NAMEDCOLLECTION:
        case OCI_TYPECODE_OBJECT:
          Nan::Set(obj, key, toJsObject(attr_tdo, attr_value, attr_null_struct));
          break;
        default:
          Nan::Set(obj, key, primitiveToJsObj(elemTypecode, attr_value));
        }
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
            int exists;
            void *elem = nullptr, *elemNull = nullptr;
            ociCall (OCICollGetElem (envh_, errh_, (OCIColl*)obj_buf, i, &exists, &elem, &elemNull), errh_);
            if (!exists)
              break;

            OCITypeCode collElemTypecode;
            ociCall (OCIAttrGet (collElemHandle, OCI_DTYPE_PARAM, &collElemTypecode, 0, OCI_ATTR_TYPECODE, errh_), errh_);

            Nan::Set(arr, i, toJsObject(collElemType, elem, elemNull));
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