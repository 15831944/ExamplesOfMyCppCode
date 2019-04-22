#ifndef __C_DIMENSIONING_LOCKEDDIMENSIONSANALIZER_H__
#define __C_DIMENSIONING_LOCKEDDIMENSIONSANALIZER_H__

#include "dbmain.h"
#include <RbCSDrawing/ExtProps/RbcsExtPropService.h>
#include "RBCSHeader\RBCSService.h"
#include "RBCSHEADER\CompositeObjectProtocol.h"

//////////////////////////////////////////////////////////////////////////////////////////
class ILockedDimensionsAnalizer
{
public:
	virtual void SetDimSnapFeature( AcDbObjectId ViewportId ) = 0;
	virtual void DeleteNeedlessDim( AcDbObjectId ViewportId ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////

class CLockedDimensionsAnalizer
{
public:
	CLockedDimensionsAnalizer();
	virtual void SetDimSnapFeature( AcDbObjectId ViewportId );
	virtual void DeleteNeedlessDim( AcDbObjectId ViewportId );

	typedef AcDbReader< AcDbObject > TReader;
	typedef AcDbEditor< AcDbObject > TEditor;

	static Acad::ErrorStatus ReadLong( TReader rSrcObject, LPCSTR cszPropName, long& out );
	static Acad::ErrorStatus ReadBool( TReader rSrcObject, LPCSTR cszPropName, bool& out );
	static Acad::ErrorStatus ReadDouble( TReader rSrcObject, LPCSTR cszPropName, double& out );
	static Acad::ErrorStatus ReadPoint2d( TReader rSrcObject, LPCSTR cszPropName, AcGePoint2d& out );
	static Acad::ErrorStatus ReadPoint3d( TReader rSrcObject, LPCSTR cszPropName, AcGePoint3d& out );
	static Acad::ErrorStatus ReadRefPoint( TReader rSrcObject, LPCSTR cszPropName, CReferencePointInfo& out );
	static Acad::ErrorStatus ReadObjectId( TReader rSrcObject, LPCSTR cszPropName, AcDbObjectId& out );

	static Acad::ErrorStatus WriteLong( TEditor eDestObject, LPCSTR cszPropName, long value );
	static Acad::ErrorStatus WriteBool( TEditor eDestObject, LPCSTR cszPropName, bool value );
	static Acad::ErrorStatus WriteDouble( TEditor eDestObject, LPCSTR cszPropName, const double& value );
	static Acad::ErrorStatus WritePoint2d( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint2d& value );
	static Acad::ErrorStatus WritePoint3d( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint3d& value );
	static Acad::ErrorStatus WriteRefPoint( TEditor eDestObject, LPCSTR cszPropName, const CReferencePointInfo& value );
	static Acad::ErrorStatus WriteObjectId( TEditor eDestObject, LPCSTR cszPropName, const AcDbObjectId& value );
private:
	class CListOfArc : public std::vector< AcGeEntity3d* >
	{
	public:
		~CListOfArc() 
		{
			iterator it = this->begin();
			iterator end_it = this->end();
			for ( ; it!=end_it; ++it )
			{
				delete *it;
			}
		}
	};
	
	typedef pair< AcGePoint2d, AcGePoint2d > TLinePoints;
	typedef std::list< TLinePoints > ListOfLinePoints;
	typedef std::vector< AcGePoint2d > ListOfExtPoints;
	
	typedef std::vector< AcDbObjectId > ListOfIdOfLinePoints;
	ListOfIdOfLinePoints aIdOfLinePoints;
	
	void GetAllNeedData( AcDbObjectIdArray& aDimId, RCAD::SetPnts2d& setPnts, CListOfArc& aArcs, ListOfLinePoints& aLinePoints, ListOfExtPoints& aExtPoints);
	bool GetRealPoints(TReader pSrcObject, AcGePoint2d&	point1, AcGePoint2d& point2);
	bool IsPointsProjectToOnePointOnMeasureLine( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point1, const AcGePoint2d& point2 );
	bool IsDimPointSnapToLine( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point, const ListOfLinePoints& aLinePoints );
	bool IsDimPointSnapToArc( const AcGePoint2d& point, const CListOfArc& aArcs );
	bool IsDimPointSnapToExt( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point, const ListOfExtPoints& aExtPoints );
	bool IsLinesFormCross( const TLinePoints& lp1, const TLinePoints& lp2);
	bool IsLineToPerpDrilling( const TLinePoints& lp, const RCAD::SetPairsPnts& setOfPerpDrillPoints );
	bool IsLineWasToPerpDrilling( const AcDbObjectId& Id);

	void DeleteDimObjs();
	void AppendDimObjToDelete( AcDbObjectId objId );
	
	AcDbObjectId m_viewportId;
	AcDbObjectIdArray m_objIds;
	bool m_beforeUpdate;
};

////
extern CLockedDimensionsAnalizer lockedDimensionsAnalizer;

struct IRbCSLockedDimensionsAnalizer
{
	virtual ILockedDimensionsAnalizer* GetLockedDimensionsAnalizer() = 0;
};

class RBCSLockedDimensionsAnalizer : public RbCSService< IRbCSLockedDimensionsAnalizer >
{
public:
	RBCSLockedDimensionsAnalizer( void );
	~RBCSLockedDimensionsAnalizer( void );
	virtual ILockedDimensionsAnalizer* GetLockedDimensionsAnalizer();
};
////

#endif //__C_DIMENSIONING_LOCKEDDIMENSIONSANALIZER_H__
