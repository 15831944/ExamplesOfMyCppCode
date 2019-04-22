/********************************************************************
	created:	2003/08/07
	created:	21:04:2003   14:53
	filename: 	U:\src\RbCSDimExt\ChainedDimensionLine.cpp
	file path:	U:\src\RbCSDimExt
	file base:	ChainedDimensionLine
	file ext:	cpp
	author:		Sergey Lavrinenko
	
	purpose:	
*********************************************************************/

// ChainedDimensionLine.cpp: implementation of the CChainedDimensionLine class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "ChainedDimensionLine.h"
#include "DrawContext2D.h"
#include "splitpointsdeclarations.h"
#include "RbCSRoot\DimStyle.hpp"
#include "RbCSHeader\common_algorithms.h"
#include "Tools/debug/DebugConfig.h"
#include "templates/acdba.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////

struct EqualPoint2d : public unary_function< AcGePoint2d, bool >
{
	EqualPoint2d( const AcGePoint2d& point ) : m_point( point ) {}
	result_type operator() ( const AcGePoint2d& pnt ) const
	{
		return m_point.isEqualTo( pnt, algo::kTol );
	}
	AcGePoint2d m_point;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CChainedDimensionLine::
CChainedDimensionLine( CDrawContext2D* _context, bool placementOptimization, bool useRefPoint ) 
: CDimensionLine( _context,placementOptimization ), m_bUseRefPoint( useRefPoint )
{
}

bool CChainedDimensionLine::drawLine(std::vector<PriorityPoint> & points,
									 const AcGeVector2d & _direction,
									 AcGePoint2d dimPoint,
									 bool toOneLine,
									 AcGePoint2d pointOnLine,
									 bool drawZeroPoint,
									 unsigned minPointsNumber,
									 bool relativeOnly,
									 ChainDimensionStyle dimStyle,
									 const AcGePoint2d& basePoint) 
{
	AcGeVector2d direction=_direction.normal();
	AcGeVector2d perpLineDirection=direction.perpVector();
	
	// >>> [ KSA : TT3255 ], 1
	//double tolerance=0.5*m_context->getDimContext()->scale();
	//------------------------
	double tolerance=0.25*m_context->getDimContext()->scale();
	// <<< [ KSA : TT3255 ], 1

	
	// YD uncomment to show 'small dimensions'
	// (see also CDimensionLine::CDimensionLine)
	// tolerance = 1e-20;
	
	if (dimStyle!=cdsBreakChain) 
	{
		//sort all points and remove those, that are too close to each other
		std::sort(points.begin(),points.end(),
			priority_point_less_functor<PriorityPoint>(direction,basePoint));

		points.erase(std::unique(points.begin(),points.end(),
			point_equal_by_projection<PriorityPoint>(direction,tolerance)),points.end());

		static bool bAdd = true;
		
		// TODO: remove this code from here, it not native for this class
		if ( m_bUseRefPoint && !relativeOnly && bAdd && ( points.size() > 2 ) )
		{
			const CReferencePointInfo& refInfo = dim::globals::GetRefPointInfo();
			AcGePoint2d refPoint = PtUtils::c3d2d( refInfo.GetRefPoint() );

			bool bRefPtIsClipped = false;

			ext::AcDb::AcDbReader<RbCSDrawingPresenter> presenter(
				dim::Environment::Instance()->GetRepresenterId() );

			if ( dim::Environment::Instance()->IsDetailView() ||
				 dim::Environment::Instance()->IsCrossSectionView())
			{
				AcGeBoundBlock2d clippingBound = dim::Environment::Instance()->GetClippingBound();

				bRefPtIsClipped = clippingBound.contains(refPoint) == false;
			}
		
			if ( bRefPtIsClipped == false )
			{
				points.push_back( refPoint );

				std::sort(points.begin(),points.end(),
					priority_point_less_functor<PriorityPoint>(direction,basePoint));
				
				points.erase(std::unique(points.begin(),points.end(),
					point_equal_by_projection<PriorityPoint>(direction,tolerance)),points.end());
			}
		}
	}

	if (points.size()<minPointsNumber)
	{
		return false;
	}

	if (dimStyle == cdsDrawNotSimplifyingOnly) 
	{
	//	>>> PC [RBCS-12271] [TT_2717] DIMENSIONING - two problems for scheme
		unsigned int numberPoints = points.size();
	//	<<< PC [RBCS-12271]
		std::vector<PriorityPoint>::iterator startit=points.begin(),
											 previt=points.begin()+1,
											 currit=previt;
		bool passingPoints=false;
		t_real distance((*previt-*startit).dotProduct(direction),tolerance);

		startit->next=true;
		for(;++currit!=points.end();previt=currit) 
		{
			double tempDist=(*currit-*previt).dotProduct(direction);				
			
			if (distance==tempDist) 
			{
				passingPoints=true;
			}
			else
			{
				if (passingPoints) 
				{
					startit=currit=points.erase(startit,previt);
					passingPoints=false;
					currit->next=true;
					(currit-1)->next=false;
					previt=currit++;
				}
				else 
				{
					startit=currit;
					previt->next=true;
				}
				
				distance=tempDist;
			}
		}
		
		if (passingPoints)
		{
			points.erase(startit+1,currit);
		}
		//	>>> PC [RBCS-12271] [TT_2717] DIMENSIONING - two problems for scheme
		if ( numberPoints > 2 )
		{
			std::vector<PriorityPoint>::iterator it_ = points.begin();
			std::vector<PriorityPoint>::iterator it2_ = points.begin();
			int count = 0;
			for ( ; it_ != points.end(); )
			{
				if ( (*it_).next )
				{
					it2_ = it_;
					count = 0;
					do
					{
						count++;
					}
					while( ++it2_ != points.end() && (*it2_).next );
					if ( it2_ == points.end() )
					{
						break;
					}
					if ( count == 1 )
					{
						it_ = points.erase( it_, it2_ );
					}
					else
					{
						it_ = it2_;
						it_++;
					}
				}
				else
				{
					++it_;
				}
			}
		}
	//	<<< PC [RBCS-12271]

		
		dimStyle=cdsBreakChain;
	}
	
	CDebugConfig cfg;
	if (cfg.Flag(_T("PROJ_DIMPOINTS_DISABLE"), _T("DEBUG_POINTS")))
		toOneLine = false;

	// cash all points for RealPoints
	typedef std::vector< std::pair< AcGePoint2d, AcGePoint2d > > RealPoints;
	RealPoints realPoints;
	bool projected = false;
	//if all points must lye on the same line they project to it
	if (toOneLine) 
	{
		projected = true;
		AcGeLine2d pointsLine(pointOnLine,direction);
	
		for(unsigned i=0;i<points.size();++i) 
		{
			AcGePoint2d tmpPoint = points[i];
			AcGeLine2d(points[i],perpLineDirection).intersectWith(pointsLine,points[i]);	
			realPoints.push_back( make_pair( points[i], tmpPoint ) );
		}
	}		

	//if line can be simplifyed, it is drawn as simplifyed
	if (dimStyle==cdsAllowSimplifyChain || dimStyle==cdsDrawSimplifyingOnly) 
	{
		if (dimStyle==cdsDrawSimplifyingOnly) 
		{
			//found distances between points
			typedef std::vector<t_real> rvector;
			rvector distances(points.size()-1,t_real(0,tolerance));
			points.front().next=true;
			for(unsigned i=1;i<points.size();++i) 
			{
				points[i].next=true;
				distances[i-1]=direction.dotProduct(points[i]-points[i-1]);
			}

			//found intervals of equal distances
			std::vector<std::vector<t_real>::iterator> iterlist;
			stl_ext::split_elements(distances.begin(),distances.end(),std::equal_to<t_real>(),iterlist);

			//calculate positions, to which correspond these intervals
			std::vector<unsigned> positions;
			
			for(unsigned i=0;i<iterlist.size()-1;++i) 
			{
				positions.push_back(iterlist[i]-distances.begin());
				positions.push_back(iterlist[i+1]-distances.begin());
			}

			//erase points
			unsigned prev=positions[positions.size()-1];
			long i=positions.size()-2;
			for(;i>=0;i-=2)
			{
				if (positions[i+1]-positions[i]>1) 
				{
					if (prev-positions[i+1]>1) 
					{
						std::vector<PriorityPoint>::iterator dstart=points.begin()+positions[i+1]+1,
															 dend=points.begin()+prev;
						dend->next=false;
						points.erase(dstart,dend);
					}
					
					prev=positions[i];
				}
			}
			if (prev>positions[i+3]) 
			{
				std::vector<PriorityPoint>::iterator dstart=points.begin()+positions[i+3],
													 dend=points.begin()+prev;
				
				dend->next=false;
				points.erase(dstart,dend);
			}
		}

		if ( projected )
		{
			RealPoints::iterator iterRP = realPoints.begin();
			RealPoints::iterator iterRP_end = realPoints.end();
			for ( ; iterRP != iterRP_end; )
			{
				std::vector<PriorityPoint>::iterator fndIter =
					std::find_if( points.begin(), points.end(), EqualPoint2d( iterRP->first ) );
				if ( fndIter == points.end() )
				{
					iterRP = realPoints.erase( iterRP );
				}
				else
				{
					++iterRP;
				}
			}
		}

		unsigned startIndex=0;
		double absolute_distance=0;
		
		for(unsigned i=startIndex; i<points.size()-1; ++i)
		{
			if (fabs(direction.dotProduct((points[startIndex+1]-points[startIndex])-(points[i+1]-points[i])))>
				0.5*m_context->getDimContext()->scale() ||
				!points[i+1].next || !points[startIndex+1].next) 
			{
			//	>>> PC [RBCS-12271] [TT_2717] DIMENSIONING - two problems for scheme
				if ( startIndex == i )
				{
					continue;
				}
			//	<<< PC [RBCS-12271]
				if ( projected )
				{
					setFirstRealPoint( realPoints[ startIndex ].second );
					setSecondRealPoint( realPoints[ i ].second );
					setHasRealPoints( true );
				}
				drawStraightDimLine(points[startIndex],points[i],dimPoint,direction,
					absolute_distance,First,false,true,(i-startIndex+1)>2 ? (i-startIndex+1) : 0);	
				
				setHasRealPoints( false );

				startIndex=i;	
			}
		}
		
		if ( projected )
		{
			setFirstRealPoint( realPoints[ startIndex ].second );
			setSecondRealPoint( realPoints.back().second );
			setHasRealPoints( true );
		}

		drawStraightDimLine(points[startIndex],points.back(),dimPoint,direction,
			absolute_distance,First,false,true,(points.size()-startIndex)>2 ? (points.size()-startIndex) : 0);
	
		setHasRealPoints( false );
		
		return true;
	}

	//iterate through points to dimension distance betwen them
	double absolute_distance=(points.front()-basePoint).dotProduct(direction);
	bool chainDrawn=false;
	
	for(unsigned i=1; i<points.size(); ++i) 
	{
		if ( projected )
		{
			setFirstRealPoint( realPoints[ i-1 ].second );
			setSecondRealPoint( realPoints[ i ].second );
			setHasRealPoints( true );
		}

		if (dimStyle==cdsBreakChain) 
		{
			if (!points[i-1].next)
			{
				continue;
			}
			
			chainDrawn|=drawStraightDimLine(points[i-1],points[i],dimPoint,direction,absolute_distance,
				Middle,false,true);
		}
		else
		{
			chainDrawn|=drawStraightDimLine(points[i-1],points[i],dimPoint,direction,absolute_distance,
				i==1 ? First : (i==points.size()-1 ? Last : Middle),
				drawZeroPoint,
				(points.size()==2) || relativeOnly);
		}
	
		setHasRealPoints( false );
	}
	
	return chainDrawn;
}


//-----------------------------------------------------------------------------


BoundsList CChainedDimensionLine::getFullBoundsList(double moveDistance) const 
{
	Acad::ErrorStatus es = Acad::eOk;
	
	BoundsList bounds;
	t_bound_box_2d block(m_boundBlock);
	block.translateBy(m_movingDirection*moveDistance);
	bounds.push_back(block);

	for(IDSList::const_iterator it=m_dimLines.begin(); it!=m_dimLines.end(); it++)
	{
		{
			AcDbObjectPointer<AcDbRotatedDimension> dimline(*it,AcDb::kForRead);

			AcGePoint2d intPoint1,intPoint2,
				linePnt1(_3d2d(dimline->xLine1Point())),
				linePnt2(_3d2d(dimline->xLine2Point())),
				dimPoint=_3d2d(dimline->dimLinePoint());

			AcGeVector2d dimLineDir(cos(dimline->rotation()),sin(dimline->rotation()));

			AcGeLine2d dimLn(_3d2d(dimline->dimLinePoint()),dimLineDir);

			dimLineDir=dimLineDir.perpVector();
			
			dimLn.intersectWith(AcGeLine2d(linePnt1,dimLineDir),intPoint1);
			dimLn.intersectWith(AcGeLine2d(linePnt2,dimLineDir),intPoint2);
						
			if (intPoint1!=linePnt1)
			{
				bounds.push_back(t_bound_box_2d(linePnt1,intPoint1-linePnt1,(intPoint1-linePnt1).perpVector().normalize()));
			}
			
			if (intPoint2!=linePnt2)
			{
				bounds.push_back(t_bound_box_2d(linePnt2,intPoint2-linePnt2,(intPoint2-linePnt2).perpVector().normalize()));
			}
		}

		AcDbObjectPointer<AcDbMText> ptr(getTextIdFromLine(*it),AcDb::kForRead);
		AcDbExtents extents;
		
		if ( ptr.openStatus() == Acad::eOk )
		{
			es = ptr->getGeomExtents(extents);
			RXASSERT(es == Acad::eOk);

			AcGePoint2d minp(_3d2d(extents.minPoint())),maxp(_3d2d(extents.maxPoint()));
			bounds.push_back(t_bound_box_2d(minp,AcGeVector2d(maxp.x-minp.x,0),AcGeVector2d(0,maxp.y-minp.y)));
		}
	}

	for(IDSList::const_iterator it=m_freeText.begin(); it!=m_freeText.end(); ++it) 
	{
		AcDbObjectPointer<AcDbMText> ptr(*it,AcDb::kForRead);
		AcDbExtents extents;
		
		if ( ptr.openStatus() == Acad::eOk )
		{
			es = ptr->getGeomExtents(extents);
			RXASSERT(es == Acad::eOk);

			AcGePoint2d minp(_3d2d(extents.minPoint())),maxp(_3d2d(extents.maxPoint()));
			bounds.push_back(t_bound_box_2d(minp,AcGeVector2d(maxp.x-minp.x,0),AcGeVector2d(0,maxp.y-minp.y)));
		}
	}
	
	return bounds;
}


//-----------------------------------------------------------------------------


void CChainedDimensionLine::move(double moveDistance) 
{
	IDSList::iterator it;

	for(it=m_dimLines.begin(); it!=m_dimLines.end(); it++) 
	{
		AcDbObjectPointer<AcDbRotatedDimension> dptr(*it,AcDb::kForWrite);
		dptr->setDimLinePoint(_2d3d(_3d2d(dptr->dimLinePoint())+m_movingDirection*moveDistance));
	}
	
	for(it=m_freeText.begin(); it!=m_freeText.end(); it++) 
	{
		AcDbObjectPointer<AcDbMText> tptr(*it,AcDb::kForWrite);
		tptr->setLocation(_2d3d(_3d2d(tptr->location())+m_movingDirection*moveDistance));
	}	
}