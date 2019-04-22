#include "StdAfx.h"
#include "LockedDimensionsAnalizer.h"
#include "RbCSDrawing\Utils.h"
#include "RBCSDRAWING\EXTPROPS\RbcsExtPropService.h"
#include "templates\geometry.h"
#include "RBCSDRAWING\DrawingPresenter.hpp"
//#include <vector>
//#include <map>
#include "PropNames.h"

CLockedDimensionsAnalizer::CLockedDimensionsAnalizer()
{
}

Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadLong( TReader rSrcObject, LPCSTR cszPropName, long& out )
{
	return rbcsExpropService->ReadLong( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadBool( TReader rSrcObject, LPCSTR cszPropName, bool& out )
{
	return rbcsExpropService->ReadBool( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadDouble( TReader rSrcObject, LPCSTR cszPropName, double& out )
{
	return rbcsExpropService->ReadDouble( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadPoint2d( TReader rSrcObject, LPCSTR cszPropName, AcGePoint2d& out )
{
	return rbcsExpropService->ReadPoint2d( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadPoint3d( TReader rSrcObject, LPCSTR cszPropName, AcGePoint3d& out )
{
	return rbcsExpropService->ReadPoint3d( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadRefPoint( TReader rSrcObject, LPCSTR cszPropName, CReferencePointInfo& out )
{
	return rbcsExpropService->ReadRefPoint( rSrcObject, cszPropName, out );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
ReadObjectId( TReader rSrcObject, LPCSTR cszPropName, AcDbObjectId& out )
{
	return rbcsExpropService->ReadObjectId( rSrcObject, cszPropName, out );
}

Acad::ErrorStatus CLockedDimensionsAnalizer::
WriteLong( TEditor eDestObject, LPCSTR cszPropName, long value )
{
	return rbcsExpropService->WriteLong( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WriteBool( TEditor eDestObject, LPCSTR cszPropName, bool value )
{
	return rbcsExpropService->WriteBool( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WriteDouble( TEditor eDestObject, LPCSTR cszPropName, const double& value )
{
	return rbcsExpropService->WriteDouble( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WritePoint2d( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint2d& value )
{
	return rbcsExpropService->WritePoint2d( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WritePoint3d( TEditor eDestObject, LPCSTR cszPropName, const AcGePoint3d& value )
{
	return rbcsExpropService->WritePoint3d( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WriteRefPoint( TEditor eDestObject, LPCSTR cszPropName, const CReferencePointInfo& value )
{
	return rbcsExpropService->WriteRefPoint( eDestObject, cszPropName, value );
}
Acad::ErrorStatus CLockedDimensionsAnalizer::
WriteObjectId( TEditor eDestObject, LPCSTR cszPropName, const AcDbObjectId& value )
{
	return rbcsExpropService->WriteObjectId( eDestObject, cszPropName, value );
}


void CLockedDimensionsAnalizer::AppendDimObjToDelete( AcDbObjectId objId )
{
	if ( !m_objIds.contains( objId ) )
	{
		AcDbObjectId tempId = objId;

		while ( tempId != AcDbObjectId::kNull )
		{
			if ( m_objIds.contains( tempId ) )
			{
				break;;
			}
			m_objIds.append( tempId );

			AcDbReader< AcDbRotatedDimension > pRotDim( tempId );
			if ( pRotDim )
			{
				AcDbObjectId nullId;
				Acad::ErrorStatus es = rbcsExpropService->ReadObjectId( pRotDim.object(), NULL_TEXT_ID, nullId );
				if ( es == Acad::eOk )
				{
					m_objIds.append( nullId );
				}
				AcDbObjectId mtextId;
				es = rbcsExpropService->ReadObjectId( pRotDim.object(), VAL_TEXT_ID, mtextId );
				if ( es == Acad::eOk )
				{
					m_objIds.append( mtextId );
				}
				AcDbObjectId nextId;
				es = rbcsExpropService->ReadObjectId( pRotDim.object(), DIM_NEXT_ID, nextId );
				if ( es == Acad::eOk )
				{
					tempId = nextId;
				}
				else
				{
					break;
				}
			}
			else
			{
				tempId = AcDbObjectId::kNull;
			}
		}
	}
}

void CLockedDimensionsAnalizer::DeleteDimObjs()
{
	if ( m_objIds.isEmpty() )
	{
		return;
	}

	AcDbEditor< RbCSViewport > pVp( m_viewportId );
	if ( pVp )
	{
		AcDbObjectIdArray dimObjs;
		pVp->GetDimObjects( dimObjs );

		unsigned int size = m_objIds.length();
		for ( unsigned int i = 0; i < size; ++i )
		{
			dimObjs.remove( m_objIds.at( i ) );
		}
		pVp->SetDimObjects( dimObjs );
	}
	
	RbCSCompositeObjectProtocol::IdsVector ids;
	unsigned int size = m_objIds.length();
	for ( unsigned int i = 0; i < size; ++i )
	{
		AcDbEntityPointer pDim( m_objIds.at( i ), AcDb::kForWrite );
		if( pDim.openStatus() == Acad::eOk )
		{
			pDim->erase();
			ids.push_back( m_objIds.at( i ) );
		}
	}
	CompletelyEraseEntities( ids );
	
	m_objIds.removeAll();
}

RBCSLockedDimensionsAnalizer::RBCSLockedDimensionsAnalizer( void )
{
}

RBCSLockedDimensionsAnalizer::~RBCSLockedDimensionsAnalizer( void )
{
}


CLockedDimensionsAnalizer lockedDimensionsAnalizer;

ILockedDimensionsAnalizer* RBCSLockedDimensionsAnalizer::GetLockedDimensionsAnalizer()
{
	return ( ILockedDimensionsAnalizer* )&lockedDimensionsAnalizer;
}

bool CLockedDimensionsAnalizer::
IsLinesFormCross( const TLinePoints& lp1, const TLinePoints& lp2 )
{
	AcGePoint2d lp1f = lp1.first;
	AcGePoint2d lp1s = lp1.second;
	AcGePoint2d lp1c = ( lp1f + lp1s.asVector() )/2;
	double lp1_len = ( lp1f - lp1s ).length();
	AcGePoint2d lp2f = lp2.first;
	AcGePoint2d lp2s = lp2.second;
	AcGePoint2d lp2c = ( lp2f + lp2s.asVector() )/2;
	double lp2_len = ( lp2f - lp2s ).length();
	AcGeLine2d l1( lp1f, lp1s );
	AcGeLine2d l2( lp2f, lp2s );
	return l1.isPerpendicularTo( l2 ) && ( lp1_len - lp2_len )<= algo::kTol.equalPoint() && ( lp1c - lp2c ).length() <= algo::kTol.equalPoint();
}

bool CLockedDimensionsAnalizer::
IsLineToPerpDrilling( const TLinePoints& lp, const RCAD::SetPairsPnts& setOfPerpDrillPoints )
{
	bool bIsAxesOfPerpDrill = false;
							
	AcGeLineSeg2d lineSeg(lp.first,lp.second);

	RCAD::SetPairsPnts::const_iterator it = setOfPerpDrillPoints.begin();
	RCAD::SetPairsPnts::const_iterator end_it = setOfPerpDrillPoints.end();

	for ( ; it != end_it; ++it )
	{
		AcGePoint2d cp = ( it->first + (it->second).asVector() )/2;
		if ( lineSeg.isOn(cp , algo::kTol ) ) 
		{
			bIsAxesOfPerpDrill = true;
			break;
		}
	}
	return bIsAxesOfPerpDrill;
}

bool CLockedDimensionsAnalizer::
IsLineWasToPerpDrilling( const AcDbObjectId& Id)
{
	bool b = false;
	ListOfIdOfLinePoints::const_iterator it = aIdOfLinePoints.begin();
	ListOfIdOfLinePoints::const_iterator end_it = aIdOfLinePoints.end();

	for ( ; it != end_it; ++it )
	{
		if ( *it == Id ) 
		{
			b = true;
			break;
		}
	}
	return b;
}

void CLockedDimensionsAnalizer::
GetAllNeedData( AcDbObjectIdArray& aDimId, RCAD::SetPnts2d& setPnts, CListOfArc& aArcs, ListOfLinePoints& aLinePoints, ListOfExtPoints& aExtPoints )
{
	RCAD::SetPairsPnts setOfPerpDrillPoints;
	RCAD::CGeArcCollection arc_set;
	{
		AcDbReader< RbCSViewport > pViewport( m_viewportId );
		if( pViewport.openStatus() != Acad::eOk )
		{
			return;
		}

		pViewport->GetDimObjects( aDimId );
		
		AcDbReader< RbCSDrawingPresenter > pPresenter( pViewport->GetRepresentorId() );
		if( pPresenter.openStatus() != Acad::eOk )
		{
			return;
		}

		pPresenter->GetAllSnapPoints( setPnts );
		
		pPresenter->GetAllPerpDrillPnts( setOfPerpDrillPoints );
		
		AcDbExtents ext;
		pPresenter->subGetGeomExtents( ext );
		AcGePoint3d maxPnt = ext.maxPoint(), minPnt = ext.minPoint();
		AcGePoint2d
		leftBottom( minPnt.x, minPnt.y ),
		leftTop( minPnt.x, maxPnt.y ),
		rightTop( maxPnt.x, maxPnt.y ),
		rightBottom( maxPnt.x, minPnt.y );
		aExtPoints.push_back(leftBottom);
		aExtPoints.push_back(leftTop);
		aExtPoints.push_back(rightTop);
		aExtPoints.push_back(rightBottom);

		arc_set = pPresenter->GetOrigGeArcSet();
		pPresenter->GetDispGeArcSet( arc_set, true, true );
	}

	RCAD::SetPnts2d setOfCenterOfArcs;

	RbCS::GeArcsArray::const_iterator it_c = arc_set.begin();
	RbCS::GeArcsArray::const_iterator it_c_end = arc_set.end();
	
	for ( ; it_c != it_c_end; ++it_c )
	{
		if ( it_c->IsKindOf( AcGe::kCircArc3d ) )
		{
			RbCS::TEntity3dPtr ent = it_c->AsEntity3d();
			AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >( ent.get() );
			AcGePoint2d	centerPoint = PtUtils::c3d2d( arc->center() );
			setPnts.insert( centerPoint );
			setOfCenterOfArcs.insert( centerPoint );
			aArcs.push_back( arc->orthoProject( AcGePlane::kXYPlane ) );

		}
	}

	int n = aDimId.length();
	
	
	for ( int dimCount=0; dimCount != n; ++dimCount )
	{
		AcDbReader< AcDbArc > pArc( aDimId[dimCount] );
		if ( pArc )
		{	
			AcGePoint3d startPoint, endPoint, midPoint;
			double startParam, endParam, midParam;
			pArc->getStartPoint( startPoint );
			pArc->getEndPoint( endPoint );
			pArc->getStartParam( startParam );
			pArc->getEndParam( endParam );
			midParam = ( startParam+endParam )/2;
			pArc->getPointAtParam( midParam, midPoint );
			AcGePoint2d	centerPoint = PtUtils::c3d2d( pArc->center() );
			setPnts.insert( centerPoint );
			setOfCenterOfArcs.insert( centerPoint );
			AcGeCircArc3d  arc( startPoint, midPoint, endPoint );  
			setPnts.insert( PtUtils::c3d2d( arc.startPoint() ) );
			setPnts.insert( PtUtils::c3d2d( arc.endPoint() ) );
			AcGeEntity3d* ent = arc.orthoProject( AcGePlane::kXYPlane );
			//AcGe::EntityId entId = ent->type();
			aArcs.push_back( ent );
		}
	}
	
	bool bIsArcListEmpty = setOfCenterOfArcs.empty();
	
	typedef std::map< AcGePoint2d, AcDbObjectId, RCAD::LessPnt2d > TLinesCenter;
	TLinesCenter mapOfLinesCenter;
	
	typedef	ListOfLinePoints::iterator  LineIt;
	typedef std::map< AcGePoint2d, LineIt, RCAD::LessPnt2d > TLinesIndex;
	TLinesIndex mapOfLinesIndex;
	TLinesIndex::iterator it_i = mapOfLinesIndex.begin();
	LineIt it_lp = aLinePoints.begin();
	
	if ( m_beforeUpdate )
	{
		aIdOfLinePoints.clear();	
	}
	
	for ( int dimCount=0; dimCount != n; ++dimCount )
	{
		AcDbObjectId DimId  = aDimId[dimCount];
		AcDbReader< AcDbLine > pLine( DimId );
		if ( pLine )
		{
			AcGePoint2d sp = PtUtils::c3d2d( pLine->startPoint() );
			AcGePoint2d ep = PtUtils::c3d2d( pLine->endPoint() );
			AcGePoint2d cp;
			TLinePoints lp1( sp , ep );
						
			bool bIsLineAdd = true;
			bool bIsLineItAdd = false;
			
			bool bIsHoleAxis=false;
		
			Acad::ErrorStatus es = rbcsExpropService->ReadBool( pLine.object(), IS_HOLE_AXIS, bIsHoleAxis );
			
			if ( es==Acad::eOk && bIsHoleAxis )
			{
				bool bIsAxesOfPerpDrill = IsLineToPerpDrilling( lp1, setOfPerpDrillPoints );
				
				if ( m_beforeUpdate && bIsAxesOfPerpDrill )
				{
					aIdOfLinePoints.push_back( DimId );	
				}
				
				if ( !m_beforeUpdate )
				{
					bool bIsLineWasToPerpDrilling = IsLineWasToPerpDrilling( DimId );
							
					if ( !bIsAxesOfPerpDrill && bIsLineWasToPerpDrilling )
					{
						bIsLineAdd = false;
						AppendDimObjToDelete( DimId );	
					}
										
					if ( !bIsLineWasToPerpDrilling && !bIsArcListEmpty )
					{
						cp = ( sp + ep.asVector() )/2;
						
						TLinesCenter::iterator it_c = mapOfLinesCenter.find( cp );
						bool bIsPointInSetOfLinesCenter = it_c != mapOfLinesCenter.end();
						
						bool bIsPointNotCenterOfArcs = setOfCenterOfArcs.find( cp ) == setOfCenterOfArcs.end();
						
						bool bIsLinesFormCross = false;
						if ( bIsPointInSetOfLinesCenter )
						{
							TLinePoints	 lp2( *mapOfLinesIndex[cp] );//lp2(*(*mapOfLinesIndex.lower_bound( cp )).second);	
							bIsLinesFormCross = IsLinesFormCross( lp1 , lp2 );
						}
						
						if ( bIsPointNotCenterOfArcs && bIsPointInSetOfLinesCenter && bIsLinesFormCross )
						{
							AppendDimObjToDelete( DimId );
							AppendDimObjToDelete( it_c->second );
							mapOfLinesCenter.erase( it_c ); 
							it_i = mapOfLinesIndex.lower_bound( cp );
							aLinePoints.erase( it_i->second );
							mapOfLinesIndex.erase( it_i ); 
							bIsLineAdd = false;
						}
						
						if ( bIsPointNotCenterOfArcs && !bIsPointInSetOfLinesCenter && !bIsLinesFormCross )
						{
							mapOfLinesCenter.insert( make_pair( cp, DimId ) );
							bIsLineItAdd = true;
						}
					}
				}	// if   not before update
			}		// if hole
					
			if ( bIsLineAdd )
			{
				aLinePoints.push_back( lp1 );
				
				if ( bIsLineItAdd ) 
				{
					it_lp = aLinePoints.end();
					--it_lp;
					mapOfLinesIndex.insert( make_pair( cp, it_lp ) );
				}
			}
		}	// if  AcDbLine
	}	// for dim
	
	DeleteDimObjs();
}

bool CLockedDimensionsAnalizer::
GetRealPoints( TReader pSrcObject, AcGePoint2d&	point1, AcGePoint2d& point2 )
{
	bool bRealPointsProp=false;
	
	Acad::ErrorStatus rb_es = rbcsExpropService->ReadBool( pSrcObject, HAS_REAL_POINTS, bRealPointsProp );
				
	bool bIsRealPointExist = false;
	
	if ( rb_es==Acad::eOk && bRealPointsProp )
	{
		Acad::ErrorStatus rp1_es = rbcsExpropService->ReadPoint2d( pSrcObject, FIRST_RP, point1 );
		Acad::ErrorStatus rp2_es = rbcsExpropService->ReadPoint2d( pSrcObject, SECOND_RP, point2 );
		bIsRealPointExist = (rp1_es == Acad::eOk) && (rp2_es == Acad::eOk);
	}
	
	if ( bIsRealPointExist )
	{
		AcDbReader< RbCSViewport > pViewport( m_viewportId );
		/*if( pViewport.openStatus() != Acad::eOk )
		{
			return;
		}*/
		
		AcDbReader< RbCSDrawingPresenter > pPresenter( pViewport->GetRepresentorId() );
		/*if( pPresenter.openStatus() != Acad::eOk )
		{
			return;
		}  */
		
		AcGePoint2dArray points;
		points.append( point1 );
		points.append( point2 );
		AcArray<bool> hiddenFlags;
		pPresenter->ToShortedLocation( points, hiddenFlags );
		point1 = points[0];
		point2 = points[1];
	}
	
	return bIsRealPointExist;
}

bool CLockedDimensionsAnalizer::
IsPointsProjectToOnePointOnMeasureLine( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point1, const AcGePoint2d& point2 )
{
	AcGeVector2d vec1( point1.x, point1.y );
	AcGeVector2d vec2( point2.x, point2.y );

	return fabs( vecOfMeasur.dotProduct( vec1 ) - vecOfMeasur.dotProduct( vec2 ) ) < algo::kTol.equalPoint();
	/*
	AcGeLine2d measurLine( AcGePoint2d::kOrigin, vecOfMeasur );
	AcGePoint2d point1_on_measure = measurLine.closestPointTo( point1 );
	AcGePoint2d point2_on_measure = measurLine.closestPointTo( point2 );
	return ( point1_on_measure - point2_on_measure ).length() <= algo::kTol.equalPoint(); 
	*/
}

bool CLockedDimensionsAnalizer::
IsDimPointSnapToLine( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point, const ListOfLinePoints& aLinePoints )
{
	bool isPointSnapToLine = false;
	ListOfLinePoints::const_iterator it_l = aLinePoints.begin();
	ListOfLinePoints::const_iterator end_it_l = aLinePoints.end();
	for ( ; it_l != end_it_l; ++it_l )
	{
		AcGePoint2d fp = it_l->first;
		AcGePoint2d sp = it_l->second;
		//AcGeLine2d line(fp,sp); 
		if ( IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, point, fp ) &&
			 IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, point, sp ) ) // || line.isOn( point ) )   // to delete?
		{
			isPointSnapToLine = true;
			break;
		}
	}
	return isPointSnapToLine;
}

bool CLockedDimensionsAnalizer::
IsDimPointSnapToArc( const AcGePoint2d& point, const CListOfArc& aArcs )
{
	bool isPointSnapToArc = false;
	CListOfArc::const_iterator it_arc = aArcs.begin();
	CListOfArc::const_iterator end_it_arc = aArcs.end();
	for ( ; it_arc != end_it_arc; ++it_arc )
	{
		/*if ( (*it_arc)->isKindOf( AcGe::kCircArc3d ))
		{
			AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >(*it_arc);
			AcGePoint3d  start = arc->startPoint();
			AcGePoint3d  end = arc->endPoint();
		}*/
		if ( (*it_arc)->isOn( PtUtils::c2d3d( point ), algo::kTol ) )
		{
			isPointSnapToArc = true;
			break;
		}
	}
	return isPointSnapToArc;
}

bool CLockedDimensionsAnalizer::
IsDimPointSnapToExt( const AcGeVector2d& vecOfMeasur, const AcGePoint2d& point, const ListOfExtPoints& aExtPoints )
{
	bool isPointSnapToExt = false;
	ListOfExtPoints::const_iterator it = aExtPoints.begin();
	ListOfExtPoints::const_iterator end_it = aExtPoints.end();
	for ( ; it != end_it; ++it )
	{
		if ( IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, *it, point ) )
		{
			isPointSnapToExt = true;
			break;
		}
	}
	return isPointSnapToExt;
}

void CLockedDimensionsAnalizer::
SetDimSnapFeature( AcDbObjectId ViewportId )
{
	m_beforeUpdate = true;
	
	m_viewportId = ViewportId;
	
	AcDbObjectIdArray aDimId;

	RCAD::SetPnts2d  setPnts;
		
	CListOfArc aArcs;

	ListOfLinePoints aLinePoints;
	
	ListOfExtPoints aExtPoints;

	GetAllNeedData( aDimId, setPnts, aArcs, aLinePoints, aExtPoints );
	
	bool bIsArcListNotEmpty = !aArcs.empty();

	std::vector< AcGePoint2d > aDimPoints;
	
	bool bIsLinePointsNotEmpty = !aLinePoints.empty();
	
	int n = aDimId.length();
	
	for ( int dimCount=0;  dimCount != n; ++dimCount )
	{
		AcDbReader< AcDbDimension > pDim( aDimId[dimCount] );

		if ( pDim )
		{
			bool bIsRotDim=false;
			
			AcGeVector2d vecOfMeasur = AcGeVector2d::kXAxis;

			std::vector< AcGePoint2d > aDimPoints;

			if ( const AcDb3PointAngularDimension* pAngDim = AcDb3PointAngularDimension::cast( pDim.object() ) )
			{
				aDimPoints.push_back ( PtUtils::c3d2d( pAngDim->centerPoint() ) );
				//aDimPoints.push_back ( PtUtils::c3d2d( pAngDim->arcPoint() ) );
			}
			else 
			{
				AcGePoint2d	realPoint1, realPoint2, snapPoint1, snapPoint2, point1, point2;
				
				bool bIsRealPointExist = GetRealPoints( pDim.object(), realPoint1, realPoint2 );
								
				if ( const AcDbRotatedDimension* pRotDim = AcDbRotatedDimension::cast( pDim.object() ) )
				{
					bIsRotDim = true;
					
					double angle = pRotDim->rotation();
					vecOfMeasur.rotateBy( angle );
					
					snapPoint1 = PtUtils::c3d2d( pRotDim->xLine1Point() );
					snapPoint2 = PtUtils::c3d2d( pRotDim->xLine2Point() );
					
					if ( bIsRealPointExist && 
						( !IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, snapPoint1, realPoint1 )
						 &&
						 !IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, snapPoint1, realPoint2 )
						 ||
						 !IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, snapPoint2, realPoint1 )
						 &&
						 !IsPointsProjectToOnePointOnMeasureLine( vecOfMeasur, snapPoint2, realPoint2 ))
						 )
					{
						bIsRealPointExist = false;
						RbCTRootUtils::UpgradeOpen uo( pDim.object() );
						rbcsExpropService->WriteBool( pDim.object(), HAS_REAL_POINTS, false );
					}
				}
				else if ( const AcDbRadialDimension* pRadDim = AcDbRadialDimension::cast( pDim.object() ) )
				{
					snapPoint1 = PtUtils::c3d2d( pRadDim->center() ) ;
					snapPoint2 = PtUtils::c3d2d( pRadDim->chordPoint() ) ;
				}
				
				if ( bIsRealPointExist )
				{
					point1 = realPoint1;
					point2 = realPoint2;
				}
				else
				{
					point1 = snapPoint1;
					point2 = snapPoint2;
				}

				aDimPoints.push_back( point1 );
				aDimPoints.push_back( point2 );
			}

			std::vector< AcGePoint2d >::iterator it_dp = aDimPoints.begin();
			std::vector< AcGePoint2d >::iterator end_it_dp = aDimPoints.end();

			bool bDimSnap=true;
			bool bIsSnapToHole = false;
			
			for ( ; it_dp != end_it_dp; ++it_dp )
			{
				if (  setPnts.find( *it_dp ) != setPnts.end() )
				{
					continue;
				}
				
				if ( bIsRotDim && bIsLinePointsNotEmpty && IsDimPointSnapToLine( vecOfMeasur, *it_dp, aLinePoints ) )  
				{
					continue;
				}
				
				if ( bIsRotDim && IsDimPointSnapToExt( vecOfMeasur, *it_dp, aExtPoints ) )  
				{
					continue;
				}

				if ( bIsArcListNotEmpty && IsDimPointSnapToArc( *it_dp, aArcs ) ) 
				{	
					continue;
				}
								
				bDimSnap = false;
				break;
			}  // for aDimPoints
			
			if ( bIsSnapToHole )
			{
				RbCTRootUtils::UpgradeOpen uo( pDim.object() );
				rbcsExpropService->WriteBool( pDim.object(), DIM_SNAP_TO_HOLE, true );
			}
			
			RbCTRootUtils::UpgradeOpen uo( pDim.object() );
			rbcsExpropService->WriteBool( pDim.object(), DIM_IS_SNAPPED, bDimSnap );		
		}  // if ( pDim )
	} // for  aDimId
}


void CLockedDimensionsAnalizer::
DeleteNeedlessDim( AcDbObjectId ViewportId )
{
	m_beforeUpdate = false;
	
	m_viewportId = ViewportId;
	
	AcDbObjectIdArray aDimId;
	
	RCAD::SetPnts2d  setPnts;
	
	CListOfArc aArcs;
	
	ListOfLinePoints aLinePoints;
	
	ListOfExtPoints aExtPoints;

	GetAllNeedData( aDimId, setPnts, aArcs, aLinePoints, aExtPoints );

	bool bIsArcListNotEmpty = !aArcs.empty();
	
	bool bIsLinePointsNotEmpty = !aLinePoints.empty();
	
	int n = aDimId.length();
	
	for ( int dimCount=0; dimCount != n; ++dimCount )
	{
		AcDbObjectId DimId = aDimId[dimCount];

		AcDbReader< AcDbDimension > pDim( DimId );

		if ( pDim )
		{
			bool bDimSnap=true;
			
			Acad::ErrorStatus rb_es_ds = rbcsExpropService->ReadBool( pDim.object(), DIM_IS_SNAPPED, bDimSnap );

			if ( (rb_es_ds == Acad::eOk) && !bDimSnap )
			{
				continue;
			}

			bool bIsRotDim=false;

			AcGeVector2d vecOfMeasur = AcGeVector2d::kXAxis;

			std::vector< AcGePoint2d > aDimPoints;

			if ( const AcDb3PointAngularDimension* pAngDim = AcDb3PointAngularDimension::cast( pDim.object() ) )
			{
				aDimPoints.push_back ( PtUtils::c3d2d( pAngDim->centerPoint() ) );
				//aDimPoints.push_back ( PtUtils::c3d2d( pAngDim->arcPoint() ) );
			}
			else
			{
				AcGePoint2d	point1, point2;
				
				const AcDbRotatedDimension* pRotDim = AcDbRotatedDimension::cast( pDim.object() );
				
				bIsRotDim = pRotDim != 0;

				if ( bIsRotDim )
				{	
					double angle = pRotDim->rotation();
					vecOfMeasur.rotateBy( angle );
				}

				if ( !GetRealPoints( pDim.object(), point1, point2 ) )
				{	
					if ( bIsRotDim )
					{	
						point1 = PtUtils::c3d2d( pRotDim->xLine1Point() );
						point2 = PtUtils::c3d2d( pRotDim->xLine2Point() );
					}
					else if ( const AcDbRadialDimension* pRadDim = AcDbRadialDimension::cast( pDim.object() ) )
					{
						point1 = PtUtils::c3d2d( pRadDim->center() ) ;
						point2 = PtUtils::c3d2d( pRadDim->chordPoint() ) ;
					}
				}
			
				aDimPoints.push_back( point1 );
				aDimPoints.push_back( point2 );
			}
			
			std::vector< AcGePoint2d >::iterator it_dp = aDimPoints.begin();
			std::vector< AcGePoint2d >::iterator end_it_dp = aDimPoints.end();
			
			//int countSnapToExt = 0;
			//int countSnapToPres = 0;
			
			for ( ; it_dp != end_it_dp; ++it_dp )
			{
				if (  setPnts.find( *it_dp ) != setPnts.end() )
				{
					//++countSnapToPres;
					continue;
				} 
				
				if ( bIsRotDim && bIsLinePointsNotEmpty && IsDimPointSnapToLine( vecOfMeasur, *it_dp, aLinePoints ) )  
				{
					continue;
				}
				
				if ( bIsRotDim && IsDimPointSnapToExt( vecOfMeasur, *it_dp, aExtPoints ) )  
				{
					//++countSnapToExt;
					continue;
				}
				
				/*if ( countSnapToExt = 1 || countSnapToPres != 1 )
				{
					continue;
				}*/

				if ( bIsArcListNotEmpty && IsDimPointSnapToArc( *it_dp, aArcs ) ) 
				{	
					continue;
				}
								
				AppendDimObjToDelete( DimId );
				break;
			}  // for aDimPoints
		}  // if ( pDim )
	} // for  aDimId	
	
	DeleteDimObjs();
}
