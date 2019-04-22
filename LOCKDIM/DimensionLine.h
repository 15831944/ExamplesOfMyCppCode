/********************************************************************
	created:	2003/08/08
	created:	9:05:2003   10:42
	filename: 	U:\src\RbCSDimExt\DimensionLine.h
	file path:	U:\src\RbCSDimExt
	file base:	DimensionLine
	file ext:	h
	author:		Sergey Lavrinenko
	
	purpose:	
*********************************************************************/

#if !defined(AFX_DIMENSIONLINE_H__05224124_03BF_4970_B358_511B9BE8A763__INCLUDED_)
#define AFX_DIMENSIONLINE_H__05224124_03BF_4970_B358_511B9BE8A763__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CDrawContext2D;

class CDimensionLine
{

public:

	enum DimensionPositionInChain {First,Middle,Last};

	static AcDbMText *createText(AcGePoint2d location,AcGeVector2d direction,AcDbObjectId textStyle,
								 const TCHAR* text,double textHeight,Adesk::UInt16 colorIndex);
	
	static AcDbObjectId getTextIdFromLine(AcDbObjectId lineId);

	CDimensionLine(CDrawContext2D* _context,bool _placementOptimization=false);
	
	bool drawStraightDimLine(
		const AcGePoint2d& pnt1,
		const AcGePoint2d& pnt2,
		const AcGePoint2d& dimPoint,
		const AcGeVector2d& direction, 
		double &absolute_distance,
		DimensionPositionInChain dimPosInChain,
		bool drawZeroPoint,
		bool relativeOnly=false,
		unsigned simplificationNumber=0); 

protected:

	bool getTextParams(AcDbObjectId textId,
					   double& textHeight,
					   double& textWidth,
					   AcDbObjectId& textStyle,
					   Adesk::UInt16& colorIndex) const;

	bool getDistanceText(tstring& distanceText, 
						 const AcDbObjectId& dimStyleId,
						 double distance,
						 unsigned int simplificationNumber) const;

	void setHasRealPoints( bool flag );
	bool hasRealPoints();

	void setFirstRealPoint( const AcGePoint2d& point );
	void setSecondRealPoint( const AcGePoint2d& point );

	AcGePoint2d getFirstRealPoint();
	AcGePoint2d getSecondRealPoint();

protected:

	t_bound_box_2d m_boundBlock;
	AcGePoint2d m_bindingPoint;
	AcGeVector2d m_movingDirection;
	bool m_biDirectional;  // not used or set to a default or anything
	bool m_placementOptimization;	

	CDrawContext2D* m_context;
	typedef std::list<AcDbObjectId> IDSList;
	IDSList m_dimLines;
	IDSList m_freeText;	

private:

	bool m_initialized;
	double m_tolerance;
	AcGePoint2d m_absTextLastPoint;

	bool m_textIsUp;

	AcGePoint2d m_ptLastLeaderPos;
	double m_fLastLeaderTextWidth;

	bool m_bHasRealPoints;
	AcGePoint2d m_firstRealPoint;
	AcGePoint2d m_secondRealPoint;
};

#endif // !defined(AFX_DIMENSIONLINE_H__05224124_03BF_4970_B358_511B9BE8A763__INCLUDED_)
