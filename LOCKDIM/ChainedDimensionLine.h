/********************************************************************
	created:	2003/08/07
	created:	21:04:2003   14:53
	filename: 	U:\src\RbCSDimExt\ChainedDimensionLine.h
	file path:	U:\src\RbCSDimExt
	file base:	ChainedDimensionLine
	file ext:	h
	author:		Sergey Lavrinenko
	
	purpose:	
*********************************************************************/

// ChainedDimensionLine.h: interface for the CChainedDimensionLine class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHAINEDDIMENSIONLINE_H__A644D86A_05BE_4F1E_8609_775448778D99__INCLUDED_)
#define AFX_CHAINEDDIMENSIONLINE_H__A644D86A_05BE_4F1E_8609_775448778D99__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IDimLineOptimizationSubject.h"
#include "DimensionLine.h"
#include "DimStylesDeclarations.h"

class CChainedDimensionLine : public IDimLineOptimizationSubject, private CDimensionLine
{	
	double straightDist,reverseDist;
	bool m_bUseRefPoint;

	virtual void getBoundBlock(double moveDistance, t_bound_box_2d& bounds) const {
		bounds=m_boundBlock;
		bounds.translateBy(m_movingDirection*moveDistance);
	}
	virtual void getDimLinePoint(double moveDistance,AcGePoint2d& dimPoint) const {
		dimPoint=this->m_bindingPoint+m_movingDirection*moveDistance;
	}
	virtual const AcGeVector2d& getMovingDirection() const {
		return m_movingDirection;
	}
	virtual double getStraightMovingDistance() const {
		return straightDist;
	}
	virtual double getReverseMovingDistance() const {
		return reverseDist;
	}
	virtual BoundsList getFullBoundsList(double moveDistance) const;
	virtual void move(double moveDistance);

public:
	CChainedDimensionLine( CDrawContext2D* _context, bool placementOptimization = false, bool useRefPoint = true );
	
	bool drawLine(std::vector<PriorityPoint> & points,
				  const AcGeVector2d & _direction,
				  AcGePoint2d dimPoint,
				  bool toOneLine,
				  AcGePoint2d pointOnLine,
				  bool drawZeroPoint=true,
				  unsigned minPointsNumber=2,
				  bool relativeOnly=false,
				  ChainDimensionStyle dimStyle=cdsSimpleChain,
				  const AcGePoint2d& basePoint=AcGePoint2d::kOrigin);

	void setDistances(double _straightDist,double _reverseDist) {
		straightDist=_straightDist;
		reverseDist=_reverseDist;
	}
};

#endif // !defined(AFX_CHAINEDDIMENSIONLINE_H__A644D86A_05BE_4F1E_8609_775448778D99__INCLUDED_)
