#include "stdafx.h"
#include "RbcsExtPropService.h"
#include "RbCSPropAccess.h"

DbPointer::
DbPointer( AcDbObject* p )
: mPtr(p)
{
}

DbPointer::
DbPointer( const AcDbObjectId& id )
{
}

DbPointer::
operator AcDbObject*()
{
	return mPtr;
}

DbPointer::
operator const AcDbObject*() const
{
	return mPtr;
}

Acad::ErrorStatus RbcsExtPropService::
ReadLong( TReader pSrcObject, LPCSTR ctszPropName, long& out )
{
	const RbCSLongProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadBool( TReader pSrcObject, LPCSTR ctszPropName, bool& out )
{
	const RbCSBoolProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadDouble( TReader pSrcObject, LPCSTR ctszPropName, double& out )
{
	const RbCSDoubleProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadPoint2d( TReader pSrcObject, LPCSTR ctszPropName, AcGePoint2d& out )
{
	const RbCSAcGePoint2dProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadPoint3d( TReader pSrcObject, LPCSTR ctszPropName, AcGePoint3d& out )
{
	const RbCSAcGePoint3dProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadRefPoint( TReader pSrcObject, LPCSTR ctszPropName, CReferencePointInfo& out )
{
	const RbCSRefPointProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( pSrcObject, ctszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
ReadObjectId( TReader rSrcObject, LPCSTR cszPropName, AcDbObjectId& out )
{
	const RbCSObjectIdProperty* ptr = 0;
	Acad::ErrorStatus es = GetPropConst( rSrcObject, cszPropName, ptr );
	if ( ptr )
		out = ptr->GetValue();
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WriteLong( TEditor pDestObject, LPCSTR ctszPropName, long value )
{
	RbCSLongProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WriteBool( TEditor pDestObject, LPCSTR ctszPropName, bool value )
{
	RbCSBoolProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WriteDouble( TEditor pDestObject, LPCSTR ctszPropName, const double& value )
{
	RbCSDoubleProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WritePoint2d( TEditor pDestObject, LPCSTR ctszPropName, const AcGePoint2d& value )
{
	RbCSAcGePoint2dProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WritePoint3d( TEditor pDestObject, LPCSTR ctszPropName, const AcGePoint3d& value )
{
	RbCSAcGePoint3dProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WriteRefPoint( TEditor pDestObject, LPCSTR ctszPropName, const CReferencePointInfo& value )
{
	RbCSRefPointProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( pDestObject, ctszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

Acad::ErrorStatus RbcsExtPropService::
WriteObjectId( TEditor eDestObject, LPCSTR cszPropName, const AcDbObjectId& value )
{
	RbCSObjectIdProperty* ptr = 0;
	Acad::ErrorStatus es = GetProp( eDestObject, cszPropName, ptr );
	if ( ptr )
		ptr->PutValue( value );
	return es;
}

