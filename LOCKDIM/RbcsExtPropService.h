#pragma once

#include <rxsrvice.h>
#include <RbCTHeader/CommonDefs.h>
#include <RbCSDimensioning/Templates/Acdba.h>

class AcDbObject;
class AcDbObjectId;
class CReferencePointInfo;

#ifdef _RBCSDRAWING_
struct __declspec(dllexport) DbPointer
#else
struct __declspec(dllimport) DbPointer
#endif
{
	DbPointer( AcDbObject* );
	DbPointer( const AcDbObjectId& );

	operator AcDbObject*();
	operator const AcDbObject*() const;

	AcDbObject* mPtr;
};


class RbcsExtPropService : public AcRxService
{
	template < class AccessorT, class PropT >
	Acad::ErrorStatus GetProp( AccessorT& obj, const char* cszPropName, PropT*& out )
	{
		RbCS::object_property< AcDbObject, PropT > prop( obj );
		out = prop.Get( cszPropName );

		Acad::ErrorStatus e = out ? Acad::eOk : Acad::eInvalidInput;
		return e;
	}

	template < class AccessorT, class PropT >
	Acad::ErrorStatus GetPropConst( AccessorT& obj, const char* cszPropName, PropT*& out ) const
	{
		RbCS::object_property< AcDbObject, PropT > prop( obj );
		out = prop.GetConst( cszPropName );

		Acad::ErrorStatus e = out ? Acad::eOk : Acad::eInvalidInput;
		return e;
	}

public:
	typedef AcDbReader< AcDbObject > TReader;
	typedef AcDbEditor< AcDbObject > TEditor;

	virtual Acad::ErrorStatus	ReadLong		( TReader rSrcObject, LPCSTR cszPropName, long& out );
	virtual Acad::ErrorStatus	ReadBool		( TReader rSrcObject, LPCSTR cszPropName, bool& out );
	virtual Acad::ErrorStatus	ReadDouble		( TReader rSrcObject, LPCSTR cszPropName, double& out );
	virtual Acad::ErrorStatus	ReadPoint2d		( TReader rSrcObject, LPCSTR cszPropName, AcGePoint2d& out );
	virtual Acad::ErrorStatus	ReadPoint3d		( TReader rSrcObject, LPCSTR cszPropName, AcGePoint3d& out );
	virtual Acad::ErrorStatus	ReadRefPoint	( TReader rSrcObject, LPCSTR cszPropName, CReferencePointInfo& out );
	virtual Acad::ErrorStatus	ReadObjectId	( TReader rSrcObject, LPCSTR cszPropName, AcDbObjectId& out );

	virtual Acad::ErrorStatus	WriteLong		( TEditor eDestObject, LPCSTR cszPropName, long value );
	virtual Acad::ErrorStatus	WriteBool		( TEditor eDestObject, LPCSTR cszPropName, bool value );
	virtual Acad::ErrorStatus	WriteDouble		( TEditor eDestObject, LPCSTR cszPropName, const double& value );
	virtual Acad::ErrorStatus	WritePoint2d	( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint2d& value );
	virtual Acad::ErrorStatus	WritePoint3d	( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint3d& value );
	virtual Acad::ErrorStatus	WriteRefPoint	( TEditor eDestObject, LPCSTR cszPropName, const CReferencePointInfo& value );
	virtual Acad::ErrorStatus	WriteObjectId	( TEditor eDestObject, LPCSTR cszPropName, const AcDbObjectId& value );
};

#define RbcsExtPropServiceName	_T("RbcsExtPropService")
#define rbcsExpropService		(static_cast<RbcsExtPropService*>(acrxServiceDictionary->at(RbcsExtPropServiceName)))
