/********************************************************************
	created:	2003/08/08
	created:	9:05:2003   10:42
	filename: 	U:\src\RbCSDimExt\DimensionLine.cpp
	file path:	U:\src\RbCSDimExt
	file base:	DimensionLine
	file ext:	cpp
	author:		Sergey Lavrinenko
	
	purpose:	
*********************************************************************/


#include "StdAfx.h"
#include "DimensionLine.h"
#include "DimensioningContext.h"
#include "DrawContext2D.h"
#include "OptimizationMap.h"
#include "BaseDimensioningInfo.h"
#include "tools/debug/DebugConfig.h"
#include <RbCSDrawing/ExtProps/RbcsExtPropService.h>
#include "PropNames.h"

//-----------------------------------------------------------------------------


CDimensionLine::CDimensionLine(CDrawContext2D* _context,bool _placementOptimization) 
	:m_textIsUp(true),
	 m_context(_context), 
	 m_initialized(false), 
	 m_fLastLeaderTextWidth( 0 ),
	 m_placementOptimization(_placementOptimization),
	 m_bHasRealPoints( false )
{
	// used to skip small dimensions (< tolerance) 
	m_tolerance=0.5*m_context->getDimContext()->scale();
}


//-----------------------------------------------------------------------------


AcDbObjectId CDimensionLine::getTextIdFromLine(AcDbObjectId lineId) 
{	
	AcDbReader<AcDbDimension> dim_line(lineId);
	if (dim_line.openStatus() != Acad::eOk)
	{
		return AcDbObjectId::kNull;
	}

	/* read all dimensioning ids from dimBlock  */
	
	const AcDbObjectId block = dim_line->dimBlockId();
	AcDbBlockTableRecordPointer ptr(block, AcDb::kForRead);

	if ( ptr.openStatus() == Acad::eOk )	
	{
		AcDbBlockTableRecordIterator* p_block_it = NULL;
		Acad::ErrorStatus es = ptr->newIterator(p_block_it);
		std::auto_ptr< AcDbBlockTableRecordIterator > block_it ( p_block_it );
		
		CHECK(es == Acad::eOk);
		if ( block_it.get() == NULL && es == Acad::eOk )
		{
			return AcDbObjectId::kNull;
		}
			
		for ( ; block_it->done() == false; block_it->step() ) 
		{
			AcDbObjectId entity_id;
			block_it->getEntityId( entity_id );

			if ( AcDbReader< AcDbMText >( entity_id ) )
				return entity_id;
		}
	}
	
	return AcDbObjectId();	
}


//-----------------------------------------------------------------------------


AcDbMText* CDimensionLine::createText(AcGePoint2d location,
									 AcGeVector2d direction,
									 AcDbObjectId textStyle,
									 const TCHAR* text,
									 double textHeight,
									 Adesk::UInt16 colorIndex) 
{
	Acad::ErrorStatus es(Acad::eOk);

	AcDbMText *textPtr = new AcDbMText();

	es = textPtr->setLocation(_2d3d(location));
	CHECK(es == Acad::eOk);

	const bool negateDirection = direction.angle()>M_PI_2 && direction.angle()<=M_1PI_2;
	es = textPtr->setDirection(_2d3d(negateDirection ?  -direction : direction));
	CHECK(es == Acad::eOk);
	
	es = textPtr->setTextStyle(textStyle);
	CHECK(es == Acad::eOk);
	
	textPtr->setContents(text);
		
	es = textPtr->setTextHeight(textHeight);
	CHECK(es == Acad::eOk);
	
	es = textPtr->setColorIndex(colorIndex);
	CHECK(es == Acad::eOk);
	
	return textPtr;
}


//-----------------------------------------------------------------------------


bool CDimensionLine::getTextParams(AcDbObjectId textId,
								   double &textHeight,
								   double &textWidth,
								   AcDbObjectId& textStyle,
								   Adesk::UInt16& colorIndex) const
{
	AcDbReader<AcDbMText> dimTextPtr(textId);

	if (dimTextPtr.openStatus() != Acad::eOk)
	{
		return false;
	}
	
	colorIndex = dimTextPtr->colorIndex();
	
	textStyle = dimTextPtr->textStyle();

	textHeight = dimTextPtr->actualHeight();
	
	textWidth = dimTextPtr->actualWidth();
	
	return true;
}


//-----------------------------------------------------------------------------


bool CDimensionLine::getDistanceText(tstring& distanceText, 
									 const AcDbObjectId& dimStyleId,
									 double distance,
									 unsigned int simplificationNumber) const
{
    AcDbReader<AcDbDimStyleTableRecord> acadstyle(dimStyleId);
	CHECK(acadstyle.openStatus() == Acad::eOk);

	if (acadstyle.openStatus() != Acad::eOk)
	{
		return false;
	}
	
	const unsigned int maxDistanceTextSize = 100;
	TCHAR text[maxDistanceTextSize]=_T("");
	
	acdbRToS(distance,
			 acadstyle->dimlunit(),
			 acadstyle->dimdec(),
			 text);

	distanceText = text;

    // for example: if @simplificationNumber - 6 and @distance - 100,
	// the result will be "5x20=100"
	if (simplificationNumber!=0 && simplificationNumber!=1) 
	{
		acdbRToS(distance/(simplificationNumber-1), 
				 acadstyle->dimlunit(), 
				 acadstyle->dimdec(),
				 text);

		distanceText = tstring(_T("x")) + text + _T("=") + distanceText;

		distanceText = _itot(simplificationNumber-1, text, 10) + distanceText;
	}

	return true;
}


//-----------------------------------------------------------------------------


bool CDimensionLine::drawStraightDimLine(
		const AcGePoint2d& point1,
		const AcGePoint2d& point2,
		const AcGePoint2d& dimPoint,
		const AcGeVector2d& _direction, 
		double &absolute_distance,
		DimensionPositionInChain dimPosInChain,
		bool drawZeroPoint,
		bool relativeOnly,
		unsigned simplificationNumber) 
{
		
	AcGePoint2d pnt1(point1),pnt2(point2),dimPnt(dimPoint);
	AcGeVector2d direction(_direction.normal());

	DimFormat format(getDimFormat(m_context->getDimContext()->viewDimStyle()->objectId()));
	
	if (format==dfToBase || relativeOnly || simplificationNumber!=0)
	{
		format=dfRelativeChain;
	}

	double distance(fabs(direction.dotProduct(pnt1-pnt2)));
	absolute_distance+=distance;

	// >>> [ KSA : TT3255 ], 2
	// It is supposed thed "small" dimensions are filtered early
	//if (distance<m_tolerance)
	//{
	//	return false;
	//}
	// <<< [ KSA : TT3255 ], 2
	
	//new points position and dimline direction considering shortenings and presentor CS
	bool visible(true),modified(false);
	
	if (!m_context->to_presentor(pnt1,pnt2,visible,modified))
	{
		return false;
	}
	
	m_context->to_presentor(direction);

	AcGeVector2d perpLineDirection=direction.perpVector(); 

	m_context->to_presentor(dimPnt,visible,modified);
	
	if (!visible) 
	{
		AcGeVector2d dir=_direction.normal().perpVector();
		dimPnt=pnt1+perpLineDirection*dir.dotProduct(dimPoint-point1);
	} 
	//----------------------------------------------------------------------------------
		
	//If line not optimizing, expand drawing bounds-------------------------------------
	if (!m_placementOptimization) 
	{
		AcGePoint2d intPoint;
		AcGeLine2d dLine(dimPoint,_direction);
		dLine.intersectWith(AcGeLine2d(point1,_direction.perpVector()),intPoint);
		m_context->expandDrawingBounds(intPoint);
		dLine.intersectWith(AcGeLine2d(point2,_direction.perpVector()),intPoint);
		m_context->expandDrawingBounds(intPoint);
	}
	//----------------------------------------------------------------------------------

	const AcDbObjectId dimStyleId = m_context->getDimContext()->dimStyle()->GetAcadDimStyleId();

	TCHAR distText[100]=_T("");
	if (format!=dfAbsoluteChain)
	{
		tstring distanceText;

		if ( getDistanceText(distanceText, dimStyleId, distance, simplificationNumber) )
		{
			_tcscpy(distText, distanceText.c_str());
		}
	}

	//distance with shortenings
	double new_dist(fabs(direction.dotProduct(pnt1.asVector()-pnt2.asVector())));
	
	//create dimension line and append to drawing

	const bool useDefaultText = fabs(distance-new_dist)<.1 && simplificationNumber==0;

	AcDbRotatedDimension *dim_line = 
		new AcDbRotatedDimension(direction.angle(),
								_2d3d(pnt1),
								_2d3d(pnt2),
								_2d3d(dimPnt),
								(format==dfAbsoluteChain) ? _T("") : (useDefaultText ? 0 : distText),
								dimStyleId);

	dim_line->setDimtix(Adesk::kTrue);
	m_context->appendDimensionToDrawing(dim_line);
	AcDbObjectId dimLineId = dim_line->objectId();
	m_dimLines.push_back( dimLineId );

	if ( hasRealPoints() )
	{
		AcDbEditor< AcDbObject > pDim_line( dimLineId );
		if ( pDim_line )
		{
			Acad::ErrorStatus es = rbcsExpropService->WriteBool( pDim_line, HAS_REAL_POINTS, true );
			es = rbcsExpropService->WritePoint2d( pDim_line, FIRST_RP, getFirstRealPoint() );
			es = rbcsExpropService->WritePoint2d( pDim_line, SECOND_RP, getSecondRealPoint() );
		}
	}

	CDebugConfig cfg;
	const bool bDraw = cfg.Flag( _T("MARK_DIMENSIONS"), _T("DIMDIRECTIONS") );
	if ( bDraw )
	{
 		AcDbEditor< AcDbRotatedDimension > pDim( dimLineId );
 		if ( pDim )
 		{
 			int color;
 			const IDimDirection* pcDir = m_context->getDimInfo()->getDimDirectionByVector( _direction, &color );
 			if ( pcDir )
 			{
 				Acad::ErrorStatus e = pDim->setColorIndex( color );
				assert( e == Acad::eOk );
 			}
 		}
	}

	AcGePoint2d pntOnLine1,pntOnLine2;
	AcGeLine2d dimensionLine(dimPnt,direction);
	dimensionLine.intersectWith(AcGeLine2d(pnt1,perpLineDirection),pntOnLine1);
	dimensionLine.intersectWith(AcGeLine2d(pnt2,perpLineDirection),pntOnLine2);

	COptimizationMap & optMap = m_context->getOptimizationMap();

	if (!m_placementOptimization)
	{
		AcGeVector2d dir((pntOnLine1-pnt1).normal()*dim_line->dimexo()*m_context->getDimContext()->scale());
		optMap.insertArea(AcGeLineSeg2d(pnt1+dir,pntOnLine1),OccupationType::StaticDimension,0);
		optMap.insertArea(AcGeLineSeg2d(pnt2+dir,pntOnLine2),OccupationType::StaticDimension,0);
		optMap.insertArea(AcGeLineSeg2d(pntOnLine2,pntOnLine1),OccupationType::StaticDimension,0);
	}
	else 
	{
		if (!m_initialized) 
		{
			m_bindingPoint=dimPnt;
			m_movingDirection=perpLineDirection;
			m_boundBlock.set(pntOnLine1,pntOnLine2-pntOnLine1,(pntOnLine2-pntOnLine1).perpVector().normalize());
			m_initialized=true;
		}

		m_boundBlock.extend(pntOnLine1);
		m_boundBlock.extend(pntOnLine2);
	}
	
	//get size of text on dimline
	double textHeight,textWidth;
	AcDbObjectId textStyle;
	Adesk::UInt16 colorIndex;
	getTextParams(getTextIdFromLine(dim_line->objectId()),textHeight,textWidth,textStyle,colorIndex);
	
	//shift for placement of absolute dimension values text
	AcGeVector2d textDirectionOnDimLine((direction.angle()>M_PI_2&&direction.angle()<=M_1PI_2) ? -direction : direction);
	bool turnFlag=textDirectionOnDimLine.angle()>0.1 &&textDirectionOnDimLine.angle()<=M_PI_2;
	AcGeVector2d perpToTextDirection=textDirectionOnDimLine.perpVector(); 
	
	if (turnFlag)
	{
		perpToTextDirection.negate();
	}

	double DIMGAP(m_context->getDimContext()->scale()*dim_line->dimgap());
	AcGeVector2d textShift(-textDirectionOnDimLine*(DIMGAP+(turnFlag ? 0 : textHeight))+
		perpToTextDirection*(DIMGAP+(turnFlag ? 
		(dim_line->dimtad()!=0 ? 0 : textHeight/2) : 
		(dim_line->dimtad()!=0 ? textHeight + DIMGAP : textHeight/2))));

	AcDbMText * zeroTextPtr=0, * absTextPtr=0;

	//draw text for absolute values separately
	AcDbObjectPointer<AcDbDimStyleTableRecord> acadstyle(dimStyleId, AcDb::kForRead);
	assert(acadstyle.openStatus()==Acad::eOk);

	if (format!=dfRelativeChain && acadstyle.openStatus()==Acad::eOk) 
	{	
        		
		// Specification:
		// Changes in dimensioning - Reference point location; Dimensioning by ordinate
		// (3. Dimensioning.) 

		// Please note that the reference point location is "absolute"
		// and does not depend on "Draw parts in actual location" checkbox.
		// Also, at present this dimensioning style is not applyed to parts 
		// that are not orthogonal. 
        // That is why @direction can be compared with x- and y-axes directly. 

		const bool xAxisSign = dim::globals::GetRefPointInfo().GetXAxisSign();
		const bool yAxisSign = dim::globals::GetRefPointInfo().GetYAxisSign();

		int direction_adjustment_factor = 1;

		if ((direction.isCodirectionalTo(AcGeVector2d::kXAxis) == Adesk::kTrue &&
				xAxisSign == false) 
				||
			(direction.isCodirectionalTo(AcGeVector2d::kYAxis) == Adesk::kTrue &&
				yAxisSign == false))
		{
			direction_adjustment_factor = -1;
		}
				
		if (dimPosInChain==First && (fabs(absolute_distance-distance)>0.1 || drawZeroPoint)) 
		{
			acdbRToS((absolute_distance-distance)*direction_adjustment_factor,
				acadstyle->dimlunit(),
				acadstyle->dimdec(),
				distText);
			
			m_absTextLastPoint=pntOnLine1+textShift;
			zeroTextPtr=createText(m_absTextLastPoint,perpLineDirection,textStyle,distText,textHeight,colorIndex);	
			m_context->appendObjectToDrawing(zeroTextPtr);

			{
				AcDbEditor< AcDbObject > pDim_line( dimLineId );
				if ( pDim_line )
				{
					rbcsExpropService->WriteObjectId( pDim_line, NULL_TEXT_ID, zeroTextPtr->objectId() );
				}
			}
			
			if (!m_placementOptimization)	
			{
				AcDbExtents extents;
				zeroTextPtr->getGeomExtents(extents);
				optMap.insertArea(extents,OccupationType::StaticDimension,0);
			}
			m_freeText.push_back(zeroTextPtr->objectId());
		}
		if (fabs(absolute_distance)>0.1 || drawZeroPoint) 
		{
			acdbRToS(absolute_distance*direction_adjustment_factor,
				acadstyle->dimlunit(),
				acadstyle->dimdec(),
				distText);

			if (fabs((m_absTextLastPoint-(pntOnLine2+textShift)).dotProduct(textDirectionOnDimLine))<textHeight) 
			{
				m_absTextLastPoint+=direction*textHeight;
			}
			else 
			{
				m_absTextLastPoint=pntOnLine2+textShift;
			}

			absTextPtr=createText(m_absTextLastPoint,perpToTextDirection,textStyle,distText,textHeight,colorIndex);
			m_context->appendObjectToDrawing(absTextPtr);

			{
				AcDbEditor< AcDbObject > pDim_line( dimLineId );
				if ( pDim_line )
				{
					rbcsExpropService->WriteObjectId( pDim_line, VAL_TEXT_ID, absTextPtr->objectId() );
				}
			}
			
			if (!m_placementOptimization)
			{
				AcDbExtents extents;
				absTextPtr->getGeomExtents(extents);
				optMap.insertArea(extents,OccupationType::StaticDimension,0);
			}
			
			m_freeText.push_back(absTextPtr->objectId());
		}
	}
	{
		AcDbObjectPointer<AcDbRotatedDimension> ptr(dim_line->objectId(), AcDb::kForWrite);
		
		if (ptr.openStatus()==Acad::eOk) 
		{

			if (format==dfAbsoluteChain) 
			{
				ptr->setDimtad(1);
				ptr->setDimensionText(_T(" "));
			}

			if (format==dfRelativeChain /*&& !m_placementOptimization*/) 
			{
				if (new_dist>textWidth+2*DIMGAP) 
				{
					m_textIsUp=true;
				}
				else 
				{
					switch (dimPosInChain)
				{
					case First:
						m_textIsUp = true;

						ptr->setDimtix(Adesk::kFalse);
						ptr->setDimjust(1);

						if (m_placementOptimization)
						{
							m_boundBlock.extend(pntOnLine1+(pntOnLine1-pntOnLine2).normal()*textWidth);
						}
						break;
					
					case Last:
						m_textIsUp = true;

						ptr->setDimtix(Adesk::kFalse);
						ptr->setDimjust(2);

						if (m_placementOptimization)
						{
							m_boundBlock.extend(pntOnLine2+(pntOnLine2-pntOnLine1).normal()*textWidth);
						}
						break;
					
					case Middle:
						if (ptr->dimtad()!=0) 
						{
							if ( new_dist*2.0 <= textWidth+2*DIMGAP )
							{
								ptr->setDimtix(Adesk::kFalse);
								ptr->setDimtmove(1);
								AcGePoint2d text_pt=_3d2d(ptr->textPosition());

								double fIndentDist = textHeight * 3.0;
								double fScale = m_context->getDimContext()->scale();

								double fShiftDist = min( 1.5 * fScale, textHeight / 2.0 );

								AcGeVector2d vLeaderOutDir = textDirectionOnDimLine.perpVector();

								{	// Change leader direction on upper/lower dimensions
									double fd1 = perpLineDirection.dotProduct( pnt1.asVector() ),
										fd2 = perpLineDirection.dotProduct( dimPnt.asVector() ),
										ft = perpLineDirection.dotProduct( text_pt.asVector() );

									// ... if text places between dimLinePoint and xLine1Point of dimension
									if ( t_real( fabs( fd2 - fd1 ), 1e-5 ) != ( fabs( ft - fd1 ) + fabs( ft - fd2 ) ) )
									{
										vLeaderOutDir = -vLeaderOutDir;
									}
								}

								AcGeVector2d moveDir = -vLeaderOutDir * fIndentDist;
								moveDir += -fShiftDist * textDirectionOnDimLine;

								text_pt += moveDir;

								// mirror leader position if no more space
								double fLeadersGap = ( text_pt - m_ptLastLeaderPos ).length();
								if ( fLeadersGap <= ( m_fLastLeaderTextWidth + 1.5 * fScale ) )
								{
									moveDir = fShiftDist * 2.0 * textDirectionOnDimLine;
									text_pt += moveDir;
								}

								ptr->setTextPosition( PtUtils::c2d3d( text_pt ) );

								m_ptLastLeaderPos = text_pt;
								m_fLastLeaderTextWidth = textWidth;
							}
							else if (m_textIsUp)
							{
								m_textIsUp=false;
							}
							else 
							{
								ptr->setDimtix( Adesk::kFalse );
								ptr->setDimtmove( 2 );
								AcGePoint2d text_pt = PtUtils::c3d2d( ptr->textPosition() );
								m_textIsUp = true;
								AcGeVector2d moveDir = -textDirectionOnDimLine.perpVector();
								text_pt += 2.0 * moveDir * AcGeLine2d( dimPnt, direction ).distanceTo( text_pt );
								ptr->setTextPosition( _2d3d( text_pt ) );
							}
						}	
						break;
					}
				}
			}
		}
	}
		
	AcDbReader<AcDbMText> dimTextPtr(getTextIdFromLine(dim_line->objectId()));

	if (dimTextPtr.openStatus()==Acad::eOk && tstring(dimTextPtr->contents())!=_T(" ")) 
	{
		AcDbExtents extents;
		dimTextPtr->getGeomExtents(extents);

		if (!m_placementOptimization)
		{
			optMap.insertArea(extents,OccupationType::StaticDimension,0);
		}
		else 
		{
			m_boundBlock.extend(_3d2d(extents.minPoint()));
			m_boundBlock.extend(_3d2d(extents.maxPoint()));
		}
	}	
	return true;
}

void CDimensionLine::setHasRealPoints( bool flag )
{
	m_bHasRealPoints = flag;
}
bool CDimensionLine::hasRealPoints()
{
	return m_bHasRealPoints;
}

void CDimensionLine::setFirstRealPoint( const AcGePoint2d& point )
{
	m_firstRealPoint = point;
}
void CDimensionLine::setSecondRealPoint( const AcGePoint2d& point )
{
	m_secondRealPoint = point;
}

AcGePoint2d CDimensionLine::getFirstRealPoint()
{
	return m_firstRealPoint;
}
AcGePoint2d CDimensionLine::getSecondRealPoint()
{
	return m_secondRealPoint;
}
