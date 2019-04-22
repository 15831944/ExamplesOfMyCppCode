// Edward "AddWord" Yablonsky

#include "StdAfx.h"
#include "DrawingPresenter.hpp"
#include "DwgPresenterBodyClbk.hpp"
#include "Viewport.h"
#include "RbCTRoot\LineInfo.h"
#include "DwgShortening.hpp"
#include "DwgPrimitiveSet.hpp"
#include "Dwg3d2dConverter.hpp"
#include "algorithm"
#include "rect_utils.hpp"
#include <gepnt2d.h>
#include <geplin2d.h>
#include <gecint2d.h>
#include <geblok3d.h>
#include "DwgShortening.hpp"
#include "IsometricViewport.h"
#include "DimetricViewport.h"
#include "2dSchemePresenter.hpp"
#include "weldseam.h"
#include "rbcsuidimstl\acdb_pointers.h"
#include "dwg_user_obj_entity.h"
#include "DbAccess.h"

#include <acgi.h>
#include <dbhatch.h>
#include "/zlib/inc/zlib.h"

#include <RbCSDimensioning/templates/acdba.h>
#include <RbCSDimensioning/templates/geometry.h>
#include "RbcsHeader\RbcsGuid.h"

bool RbCSDrawingPresenter::s_bUseIntImprove = true;
bool RbCSDrawingPresenter::s_bSmoothArcs = false;

CGeDataAccessor* CGeDataAccessor::s_pSinglGeDataAccessor = 0;


#ifdef _DEBUG
__int64 RbCSDrawingPresenter::num_explodes = 0;
__int64 RbCSDrawingPresenter::time_explodes = 0;

__int64 RbCSDrawingPresenter::num_osnaps = 0;
__int64 RbCSDrawingPresenter::time_osnaps = 0;

__int64 RbCSDrawingPresenter::num_explArcs = 0;
__int64 RbCSDrawingPresenter::time_explArcs = 0;
#endif

class CStateObserver
{
public:
	typedef size_t State;

	void SetStateChanged() { ++m_State; }
	State GetState() const { return m_State; }
	CStateObserver() { /*It is not necessary to init m_State*/ }
	CStateObserver( State initial ) : m_State(initial) {}

private:
	State m_State;
};

bool RbCSDrawingPresenter::s_bEnableSwapping = 0;
CStateObserver gPresenterStateObserver;
AcDbEntity* RbCSDrawingPresenter::m_pEntity = NULL;

#if defined(_ACAD2004)
	#pragma comment(lib, "U:/zlib/lib/zlibstatnet.lib")
#else
	#pragma comment(lib, "U:/zlib/lib/zlibstat.lib")
#endif

AcGePoint2d min_point(const AcGePoint2d& first, const AcGePoint2d& second)
{
	return AcGePoint2d(std::_cpp_min<double>(first.x, second.x),
			std::_cpp_min<double>(first.y, second.y));
}

AcGePoint2d max_point(const AcGePoint2d& first, const AcGePoint2d& second)
{
	return AcGePoint2d(std::_cpp_max<double>(first.x, second.x),
			std::_cpp_max<double>(first.y, second.y));
}

ACRX_DXF_DEFINE_MEMBERS(RbCSDrawingPresenter, RbCSDrawingEntity,
		AcDb::kDHL_CURRENT, AcDb::kMReleaseCurrent,
		AcDbProxyEntity::kNoOperation,
		RBCSDRAWINGPRESENTER, RbCSDrawing );

Adesk::UInt16 RbCSDrawingPresenter::s_uVersion = 21;

RbCSDrawingPresenter::RbCSDrawingPresenter()
:	m_HiddenLinesIsVisible(true), 
	m_IsClipped(false), 
	m_uReadedVersion( 0 ),
	m_minGSArcMarker( 0 ),
	m_bNewbie( 1 ),
	m_mxRdTransform( AcGeMatrix3d::kIdentity )
{
}

RbCSDrawingPresenter::~RbCSDrawingPresenter()
{
	this->EraseAll();
	delete m_pEntity;
	m_pEntity = NULL;
}

Acad::ErrorStatus RbCSDrawingPresenter::
subGetClassID(CLSID* pClsid) const
{
    assertReadEnabled();

    RBCTString clsidStr;
    clsidStr.Format( _T( "{%s}" ), _T( GUID_SM_RbCSDrawingPresenter ) );
    ::CLSIDFromString( clsidStr, pClsid );

    return Acad::eOk;
}

Acad::ErrorStatus RbCSDrawingPresenter::
applyPartialUndo(AcDbDwgFiler* UndoFiler, AcRxClass* Class)
{
	if (Class != desc())
		return RbCSDrawingEntity::applyPartialUndo(UndoFiler, Class);

	TUndoOpCode UndoOpCode = uocNop;
	UndoFiler->readItem((long*)&UndoOpCode);

	Acad::ErrorStatus es = Acad::eOk;

	switch (UndoOpCode)
	{
		case uocCreate:
		{
			m_ObjectId = AcDbObjectId::kNull;
			m_ViewportId = AcDbObjectId::kNull;

			break;
		}

		case uocTransformBy:
		{
			AcGeMatrix3d XForm;
			es = AcGeDwgIO::inFields(UndoFiler, XForm);
			assert(es == Acad::eOk);

			es = transformBy(XForm.invert());
			assert(es == Acad::eOk);
			
			break;
		}

		case uocSetViewDir:
		{
			AcGeVector3d Vector;
			es = AcGeDwgIO::inFields(UndoFiler, Vector);
			assert(es == Acad::eOk);

			SetViewDir(Vector);

			break;
		}

		case uocSetUpVector:
		{
			AcGeVector3d Vector;
			es = AcGeDwgIO::inFields(UndoFiler, Vector);
			assert(es == Acad::eOk);

			SetUpVector(Vector);

			break;
		}

		case  uocSetHiddenLinesIsVisible:
		{
			bool Value;
			es = UndoFiler->readItem(&Value);
			assert(es == Acad::eOk);

			this->HiddenLinesIsVisible = Value;

			break;
		}

		case uocAddShortening:
		{
			AcDbSoftPointerId Id;

			es = UndoFiler->readItem(&Id);
			assert(es == Acad::eOk);

			this->RemoveShortening(Id);

			break;
		}

		case uocRemoveShortening:
		{
			AcDbSoftPointerId Id;

			es = UndoFiler->readItem(&Id);
			assert(es == Acad::eOk);

			this->AddShortening(Id);

			break;
		}

		case uocRemoveAllShortenings:
		{
			this->RestoreAllShortenings(UndoFiler);

			break;
		}

		case uocRestoreAllShortenings:
		{
			this->RemoveAllShortenings();

			break;
		}

		case uocSetSchemeId:
		{
			AcDbSoftPointerId Id;
			es = UndoFiler->readItem(&Id);
			assert(es == Acad::eOk);

			this->SetSchemeId(Id);

			break;
		}

		case uocSetWeldSeams:
		{
			es = this->LoadWeldSeams(UndoFiler);
			assert(es == Acad::eOk);

			break;
		}

		default:
			assert(0);
	}

	return UndoFiler->filerStatus();
}

void RbCSDrawingPresenter::
EnableSwapping()
{
	if ( s_bEnableSwapping )
	{
		TCHAR tchSwpName[16];
		_sntprintf( tchSwpName, 
			sizeof( tchSwpName ) / sizeof( tchSwpName[0] ),
			_T("%0d.dps"), objectId().asOldId() );

		m_aGeObjects.EnableSwapping( tchSwpName );
	}
}

bool IsLeftView( const AcGeVector3d& vX, const AcGeVector3d& vY )
{
	return	// V(x,z)
		::fabs( vX.x ) > AcGeContext::gTol.equalPoint() &&
		::fabs( vX.y ) <= AcGeContext::gTol.equalPoint() &&
		::fabs( vX.z ) <= AcGeContext::gTol.equalPoint() &&
		::fabs( vY.x ) <= AcGeContext::gTol.equalPoint() &&
		::fabs( vY.y ) <= AcGeContext::gTol.equalPoint() &&
		::fabs( vY.z ) > AcGeContext::gTol.equalPoint();
}

bool IsIsometricView( const AcGeVector3d& v )
{
	return 
		::fabs( v.x ) > AcGeContext::gTol.equalPoint() &&
		::fabs( v.y ) > AcGeContext::gTol.equalPoint() &&
		::fabs( v.z ) > AcGeContext::gTol.equalPoint();
}

AcGeMatrix3d GetViewportTransform( const AcGeVector3d& rX, const AcGeVector3d& rY )
{
	AcGeMatrix3d mx;
	AcGeVector3d vX( rX );
	AcGeVector3d vY( rY );

	if ( IsIsometricView( vX ) )
	{
		//mx.setCoordSystem( AcGePoint3d::kOrigin, m_UpVector.crossProduct( -m_ViewDir ), m_UpVector, -m_ViewDir );
		mx = AcGeMatrix3d::alignCoordSys(
			 AcGePoint3d::kOrigin, vY.crossProduct(-vX),
			 vY, -vX, AcGePoint3d::kOrigin,
			 AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
			 AcGeVector3d::kZAxis);
	}
	else
	{
		if ( IsLeftView( vX, vY ) )
		{
			// fix coord system for left view
			vX = AcGeVector3d::kYAxis;
			vY = AcGeVector3d::kXAxis;
		}
		AcGeVector3d vNewX = vX.crossProduct( vY ).normalize();
		mx.setCoordSystem( AcGePoint3d::kOrigin, vNewX, vY, vX );
	}
	return mx;
}

Acad::ErrorStatus RbCSDrawingPresenter::
dwgInFields(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	Acad::ErrorStatus es = RbCSDrawingEntity::dwgInFields(Filer);
	assert(es == Acad::eOk);
	if (es != Acad::eOk)
		return es;

	this->assertWriteEnabled();

	AcDb::FilerType iType = Filer->filerType();

	if( iType == AcDb::kFileFiler )
	{
		if (this->layerId().isValid() == false)
		{
			this->setDatabaseDefaults();
		}
	}

	this->EraseAll();

	es = Filer->readUInt16( &m_uReadedVersion );
	assert(es == Acad::eOk);

	if( m_uReadedVersion > s_uVersion )
		return Acad::eMakeMeProxy;

	m_ObjectId.setNull();
	es = Filer->readItem(&m_ObjectId);
	assert(es == Acad::eOk);

	m_ViewportId.setNull();
	es = Filer->readItem(&m_ViewportId);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::inFields(Filer, m_OCS);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::inFields(Filer, m_ViewDir);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::inFields(Filer, m_UpVector);
	assert(es == Acad::eOk);

	if ( m_uReadedVersion >= 21 )
	{
		es = m_aGeObjects.dwgInFields( Filer );
		if ( m_aGeObjects.IsValid() == false )
		{
			m_aGeObjects.Clear( false ); // save "invalid" flag
			
			// All invalid presenters must be regenerated in 
			// kLoadDwgMsg event handler (RbCSDrawing.cpp)
			if ( objectId().isResident() ) {
				gInvPresenterIdKeeper.insert( objectId() );
			}
		}
	}
	else if ( m_uReadedVersion >= 16 && m_uReadedVersion < 21 )
	{
		RCAD::CGeDataCollection geData;
		es = geData.dwgInFields( Filer );
		m_aGeObjects.SetValid( false ); // It was an ntermediate developed version
		gInvPresenterIdKeeper.insert( objectId() );
		return Acad::eMakeMeProxyAndResurrect;
	}
	else if ( m_uReadedVersion >= 12 && m_uReadedVersion < 16 )
	{
		RCAD::CGeDataCollection geData;
		geData.dwgInFields( Filer );
		if ( geData.IsValid() == false )
		{
			m_aGeObjects.SetValid( false );
			
			// All invalid presenters must be regenerated in 
			// kLoadDwgMsg event handler (RbCSDrawing.cpp)
			if ( objectId().isResident() ) {
				gInvPresenterIdKeeper.insert( objectId() );
			}
		}
		else
		{
			AcGeMatrix3d mx = GetViewportTransform( m_ViewDir, m_UpVector );
			m_aGeObjects.SetCopyTransform( mx );

			m_aGeObjects.CopyData( geData );
		}
	}
	else if (m_uReadedVersion >= 11)
	{
		es = this->readGraphObjSet_small_blocks(Filer);
		assert(es == Acad::eOk);
	} 
	else if (m_uReadedVersion >= 10)
	{
		es = this->readGraphObjSet(Filer);
		assert(es == Acad::eOk);
	}
	else if( m_uReadedVersion >= 2 )
	{
		es = this->LoadGraphObjSet2(Filer);
		assert(es == Acad::eOk);
	}
	else
	{
		es = this->LoadGraphObjSet(Filer);
		assert(es == Acad::eOk);
	}

	EnableSwapping();

	if (m_uReadedVersion >= 6)
	{
		es = this->readShortenings(Filer);
		assert(es == Acad::eOk);
	}
	else
	{
		es = this->LoadShortenings(Filer);
		assert(es == Acad::eOk);
	}

	if( m_uReadedVersion >= 3 )
	{
		es = Filer->readItem(&m_IsClipped);
		assert(es == Acad::eOk);

		if (m_IsClipped == true)
		{
			es = m_ClipBound.In(Filer);
			assert(es == Acad::eOk);
		}
	}

	if( m_uReadedVersion >= 4 )
	{
		es = Filer->readItem(&m_SchemeId);
		assert(es == Acad::eOk);
	}

	if( m_uReadedVersion >= 5 )
	{
		if( iType == AcDb::kFileFiler )
		{
			es = this->LoadWeldSeams(Filer);
			assert(es == Acad::eOk);
		}
	}

	if (m_uReadedVersion >= 7)
	{
		es = myHatch.read(Filer);
		assert(es == Acad::eOk);

		AcDbHardPointerId hatchId;
		es = Filer->readHardPointerId(&hatchId);
		assert(es == Acad::eOk);
		myHatchId = hatchId;
	}

	if (m_uReadedVersion == 8)
	{
		es = this->readUserObjEntries(Filer);
		assert(es == Acad::eOk);

		/*if (Filer->filerType() == AcDb::kFileFiler)
		{
			this->convertUserObjEntries();
		}*/
	}

	if (m_uReadedVersion >= 9)
	{
		es = this->readDwgUserObjEntIds(Filer);
		assert(es == Acad::eOk);
	}

	if ( m_uReadedVersion >= 13 )
	{
		AcGePoint3d ptMin, ptMax;
		
		es = AcGeDwgIO::inFields( Filer, ptMin );
		assert( es == Acad::eOk );
		
		es = AcGeDwgIO::inFields( Filer, ptMax );
		assert( es == Acad::eOk );

		myExtents.set( ptMin, ptMax );
	}

	if ( m_uReadedVersion >= 14 )
	{
		es = AcGeDwgIO::inFields( Filer, m_externalContour );
		assert( es == Acad::eOk );
	}

	if ( m_uReadedVersion >= 15 )
	{
		es = m_circs.dwgInFields( Filer );
		assert(es == Acad::eOk);
		
		if ( m_uReadedVersion < 20 )
		{
			AcGeMatrix3d mx = GetViewportTransform( m_ViewDir, m_UpVector );
			m_circs.TransformBy( mx );
		}
	}

	if ( m_uReadedVersion >= 16 )
	{
		es = m_aDrillSegCash.dwgInFields( Filer );
	}

	es = Filer->filerStatus();
	assert(es == Acad::eOk);
	return( es );
}

Acad::ErrorStatus RbCSDrawingPresenter::
dwgOutFields(AcDbDwgFiler* Filer) const
{
	assert(Filer != NULL);
	assert(Filer->filerStatus() == Acad::eOk);

	Acad::ErrorStatus es = RbCSDrawingEntity::dwgOutFields(Filer);
	if (es != Acad::eOk)
		return es;

	AcDb::FilerType iType = Filer->filerType();

	this->assertReadEnabled();

	es = Filer->writeUInt16(s_uVersion);
	assert(es == Acad::eOk);

	es = Filer->writeItem(m_ObjectId);
	assert(es == Acad::eOk);

	es = Filer->writeItem(m_ViewportId);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::outFields(Filer, m_OCS);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::outFields(Filer, m_ViewDir);
	assert(es == Acad::eOk);

	es = AcGeDwgIO::outFields(Filer, m_UpVector);
	assert(es == Acad::eOk);

	//es = this->SaveGraphObjSet2(Filer);
	//es = this->writeGraphObjSet(Filer);
	//es = this->writeGraphObjSet_small_blocks(Filer);
	es = m_aGeObjects.dwgOutFields( Filer );
	assert(es == Acad::eOk);

	es = this->writeShortenings(Filer);
	assert(es == Acad::eOk);

	es = Filer->writeItem(m_IsClipped);
	assert(es == Acad::eOk);

	if (m_IsClipped == true)
	{
		es = m_ClipBound.Out(Filer);
		assert(es == Acad::eOk);
	}

	es = Filer->writeItem(m_SchemeId);
	assert(es == Acad::eOk);

	if( iType == AcDb::kFileFiler || iType == AcDb::kPurgeFiler )
	{
		es = this->SaveWeldSeams(Filer);
		assert(es == Acad::eOk);
	}

	es = myHatch.write(Filer);
	assert(es == Acad::eOk);

	es = Filer->writeHardPointerId(myHatchId);
	assert(es == Acad::eOk);

	es = this->writeDwgUserObjEntIds(Filer);
	assert(es == Acad::eOk);

	AcGePoint3d pt;

	pt = myExtents.minPoint(); 
	es = AcGeDwgIO::outFields( Filer, pt );
	assert( es == Acad::eOk );

	pt = myExtents.maxPoint(); 
	es = AcGeDwgIO::outFields( Filer, pt );
	assert( es == Acad::eOk );

	AcGePolyline2d tmpPolyLine = m_externalContour;
	es = AcGeDwgIO::outFields( Filer, tmpPolyLine );
	assert( es == Acad::eOk );

	// version 15
	es = m_circs.dwgOutFields( Filer );
	assert(es == Acad::eOk);
	//

	// version 16
	es = m_aDrillSegCash.dwgOutFields( Filer );
	assert(es == Acad::eOk);
	//

	es = Filer->filerStatus();
	assert(es == Acad::eOk);
	return( es );
}

Acad::ErrorStatus RbCSDrawingPresenter::
subExplode(AcDbVoidPtrArray& EntitySet) const
{
	/*AcGeMatrix3d Transf = m_OCS * AcGeMatrix3d::worldToPlane(AcGePlane(
			AcGePoint3d::kOrigin, - m_ViewDir)).preMultBy(AcGeMatrix3d::
			projection(AcGePlane::kXYPlane, AcGeVector3d::kZAxis));*/
	AcGeMatrix3d Transf = m_OCS;

//	if ((this->Is3dView() == true) && (!RbCSDrawingPresenter::bUseIntImprove))
//	{
//
//		// Fix bug TT#3218
//		/*
//		Transf.preMultBy(AcGeMatrix3d::alignCoordSys(
//		AcGePoint3d::kOrigin, m_UpVector.crossProduct(-m_ViewDir),
//		m_UpVector,-m_ViewDir, AcGePoint3d::kOrigin,
//		AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
//		AcGeVector3d::kZAxis));
//		*/
//		AcGeMatrix3d transf = AcGeMatrix3d::alignCoordSys(
//			AcGePoint3d::kOrigin, m_UpVector.crossProduct(-m_ViewDir),
//			m_UpVector,-m_ViewDir, AcGePoint3d::kOrigin,
//			AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
//			AcGeVector3d::kZAxis);
//
//		Transf *= transf;
//		// end Fix bug TT#3218
//	}

	CLineInfo LineInfo;

	AcDbObjectPointer<RbCSViewport> Viewport(m_ViewportId, AcDb::kForRead);

	if (Viewport.openStatus() == Acad::eOk)
		Viewport->GetHiddenLineInfo(LineInfo);

	//TGraphObjSet::TConstIterator Iter = m_GrphObjSet.Begin();
	//TGraphObjSet::TConstIterator End = m_GrphObjSet.End();
	//for (; Iter != End; Iter++)

	RCAD::CGeDataCollection2 displayed_set = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );

	this->resolveHolesPrimitivesVisibility( displayed_set );

	RCAD::CGeDataCollection2::ReaderPtr rd = displayed_set.GetReader();
	size_t nSize = rd->GetSize();

	// Painted User Parts
	bool showUserParts = true;
	AcDbObjectId userPartsLineTypeId;
	Adesk::UInt16 userPartsLineColour = 0;
	AcDb::LineWeight userPartsLineWeight = AcDb::kLnWt000;
	//

	// Painted Machining
	bool machPresentation = false;
	AcDbObjectId machPartsLineTypeId;
	Adesk::UInt16 machPartsLineColour = 0;
	AcDb::LineWeight machPartsLineWeight = AcDb::kLnWt000;
	//
	{RbCSDimStylePtr dimStyle(Viewport->GetDimStyleId(), AcDb::kForRead);
	if (dimStyle.openStatus() == Acad::eOk)
	{
		showUserParts = dimStyle->getUserParts();
		userPartsLineTypeId = dimStyle->getUserPartsLineTypeId();
		userPartsLineColour = dimStyle->getUserPartsLineColour();
		userPartsLineWeight = dimStyle->getUserPartsLineThickness();
	}}
	{RbCSSinglePartDimStylePtr dimStyleSP( Viewport->GetDimStyleId(), AcDb::kForRead );
	if ( dimStyleSP.openStatus() == Acad::eOk )
	{
		machPresentation = dimStyleSP->getMachiningPresentation() == RbCSSinglePartDimStyle::mpDistinct;
		machPartsLineTypeId = dimStyleSP->getMachiningLineType();
		machPartsLineColour = dimStyleSP->getMachiningLineColour();
		machPartsLineWeight = dimStyleSP->getMachiningLineWeight();
	}}

	for ( unsigned i = 0; i < nSize; i++ )
	{
		//RCAD::TGeObjectBase* GeObject = Iter->second;
		const RbCS::CGeObject2d& geObject = (*rd)[i];

		if ( geObject.IsVisible() == false )
			continue;

		if ( ( geObject.IsHidden() == true ) && ( LineInfo.m_bVisible == false ) )
			continue;

		if ( ( geObject.GetTag() == RCAD::stGrate || geObject.GetTag() == RCAD::stUserObj ) &&
			( showUserParts == false ) )
			continue;

		AcDbEntity* pNewEntity = 0;

		switch ( geObject.GetGeType() )
		{
		case AcGe::kLineSeg2d:
		case AcGe::kLineSeg3d:
			{
				AcGePoint3d s = AcGePoint3d( AcGePlane::kXYPlane, geObject.GetPoints().first );
				AcGePoint3d e = AcGePoint3d( AcGePlane::kXYPlane, geObject.GetPoints().second );

				pNewEntity = new AcDbLine( 
					s.transformBy( Transf ), 
					e.transformBy( Transf ) );
				
				assert( pNewEntity != 0 );

				break;
			}
		/*case AcGe::kCircArc2d:
//			assert( !"TODO: need implementation here!" );
			{
				AcGePoint3d center = geObject.GetPoints().first;

				AcGeCircArc3d circ( 
					AcGePoint3d( center.x, center.y, 0.0 ),
					-AcGeVector3d::kZAxis,
					center.z );
				circ.transformBy( Transf );

				pNewEntity = new AcDbCircle(
					circ.center(),
					circ.normal(),
					circ.radius() ); 
				assert( pNewEntity != 0 );
			}
			break;
		case AcGe::kCircArc3d:
			{
				std::auto_ptr< AcGeEntity3d > e3d = geObject.AsEntity3d();
				AcGeCircArc3d* pCircle3d = static_cast< AcGeCircArc3d* >( e3d.get() );
				assert( pCircle3d != 0 );

				pCircle3d->transformBy( Transf );

				pNewEntity = new AcDbCircle( pCircle3d->center(),
					pCircle3d->normal(),
					pCircle3d->radius() ); 
				assert( pNewEntity != 0 );
			}
			break;*/

		default:
			assert( false );
		}

		if ( pNewEntity != 0 )
		{
			pNewEntity->setPropertiesFrom( this );

			if ( geObject.IsHidden() )
			{
				pNewEntity->setColorIndex( LineInfo.m_iColorIndex );
				pNewEntity->setLinetype( LineInfo.m_LineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( LineInfo.m_iLineWeight );
			}
			else if ( geObject.GetTag() == RCAD::stGrate || geObject.GetTag() == RCAD::stUserObj )
			{
				pNewEntity->setColorIndex( userPartsLineColour );
				pNewEntity->setLinetype( userPartsLineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( userPartsLineWeight );
			}
			else if ( machPresentation && ( geObject.GetTag() == RCAD::stDrillingStraight ||
				geObject.GetTag() == RCAD::stDrillingCircle || geObject.GetTag() == RCAD::stMachining ) )
			{
				pNewEntity->setColorIndex( machPartsLineColour );
				pNewEntity->setLinetype( machPartsLineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( machPartsLineWeight );
			}

			EntitySet.append( pNewEntity );
		}
	}

	explodeArcs( EntitySet );

	AcDbObjectId LinetypeId = RbCSDwgShortening::GetShortBreakLineStyleId(
			this->ViewportId);
	// fix bug 3008
	Adesk::UInt16 profileAxesColor = RbCSDwgShortening::GetPrfAxesColor(
			this->ViewportId);
	// end 3008

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; ++Iter)
	{
		TDwgShorteningPtr Shortening(*Iter, AcDb::kForRead);
		
		if ( Shortening.openStatus() != Acad::eOk )
		{
			assert( false );
			continue;
		}

		RbCSDwgShortening::T3dLinesVector Lines;
		Shortening->GetPresentationLines(Lines);

		RbCSDwgShortening::T3dLinesVector::const_iterator LinesIter =
				Lines.begin();
		RbCSDwgShortening::T3dLinesVector::const_iterator LinesEnd =
				Lines.end();
		for (; LinesIter != LinesEnd; ++LinesIter)
		{
			AcDbLine* pNewLine = new AcDbLine;
			assert( pNewLine !=0 );

			pNewLine->setPropertiesFrom(this);

			pNewLine->setLinetype(LinetypeId);
			// fix bug 3008
			pNewLine->setColorIndex( profileAxesColor );
			// end 3008

			pNewLine->setStartPoint(LinesIter->startPoint());
			pNewLine->setEndPoint(LinesIter->endPoint());

			EntitySet.append( pNewLine );
		}
	}

	//Add hatch
	AcDbReader< AcDbHatch > origHatch( myHatchId );
	if ( origHatch )
	{
		AcDbHatch* pHatch = new AcDbHatch();
		pHatch->copyFrom( origHatch.object() );

		EntitySet.append( pHatch );
	}
	
	return Acad::eOk;
}

void RbCSDrawingPresenter::
getEcs(AcGeMatrix3d& RetVal) const
{
	this->assertReadEnabled();

	RetVal = m_OCS;
}

void RbCSDrawingPresenter::
getExtents(AcDbExtents& Extents) const
{
	using namespace RbCS;
	this->assertReadEnabled();
	if ( PtUtils::Less( myExtents.minPoint(), myExtents.maxPoint() ) )
	{
		Extents.set( myExtents.minPoint(), myExtents.maxPoint() );
		return;
	}

	RCAD::CGeDataCollection2 displayed_set = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );
	RCAD::CGeDataCollection2::ReaderPtr rd = displayed_set.GetReader();

	const size_t cnSize = rd->GetSize();
	for (unsigned int i = 0; i < cnSize; i++)
	{
		const CGeObject2d& geObject = (*rd)[i];

		AcGeInterval interval;

		//if ( geObject.IsKindOf( AcGe::kCurve2d ) == true )
		if ( geObject.GetGeType() == AcGe::kLineSeg2d )
		{
			TEntity2dPtr e2d = geObject.AsEntity2d();
			const AcGeCurve2d* pCurve = static_cast< const AcGeCurve2d* >( e2d.get() );
			assert( pCurve );

			pCurve->getInterval( interval );

			AcGePoint2d lower_bound = pCurve->evalPoint( interval.lowerBound() );
			AcGePoint2d upper_bound = pCurve->evalPoint( interval.upperBound() );

			Extents.addPoint( AcGePoint3d( lower_bound.x, lower_bound.y, 0.0 ) );
			Extents.addPoint( AcGePoint3d( upper_bound.x, upper_bound.y, 0.0 ) );
		}
		/*
		if ( geObject.IsKindOf( AcGe::kCurve3d ) == true )
		{
			TEntity3dPtr e3d = geObject.AsEntity3d();
			const AcGeCurve3d* pCurve = static_cast< const AcGeCurve3d* >( e3d.get() );
			assert( pCurve );

			AcGeBoundBlock3d BoundBlock = pCurve->orthoBoundBlock();

			AcGePoint3d Min, Max;
			BoundBlock.getMinMaxPoints( Min, Max );

			Extents.addPoint( Min );
			Extents.addPoint( Max );
		}
		*/
	}

	{
	AcDbObjectPointer< RbCSViewport > Viewport( m_ViewportId, AcDb::kForRead);
	AcGeMatrix3d mtrx;// = Transf;
	if ( Viewport.openStatus() == Acad::eOk && this->Is3dView() != true )
	{
		AcGeMatrix3d tmpMatr;
		AcGeVector3d vDir, uDir;
		Viewport->getProjComponents( tmpMatr, vDir, uDir );
		mtrx = mtrx.postMultBy( tmpMatr );
	}

	RCAD::CGeArcCollection arc_set = GetOrigGeArcSet();
	GetDispGeArcSet( arc_set, true, true );

	RbCS::GeArcsArray::const_iterator it_circ = arc_set.begin();
	RbCS::GeArcsArray::const_iterator it_circ_end = arc_set.end();

	for ( ; it_circ != it_circ_end; ++it_circ )
	{
		if ( it_circ->IsKindOf( AcGe::kCircArc3d ) )
		{
			RbCS::TEntity3dPtr ent = it_circ->AsEntity3d();
			AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >( ent.get() );
			arc->transformBy( mtrx );

			AcGeBoundBlock3d BoundBlock = arc->orthoBoundBlock();

			AcGePoint3d Min, Max;
			BoundBlock.getMinMaxPoints( Min, Max );

			Extents.addPoint( Min );
			Extents.addPoint( Max );
		}
	}}

	/*if (this->Is3dView() == true)
	{
		Extents.transformBy(AcGeMatrix3d::alignCoordSys(AcGePoint3d::kOrigin,
				m_UpVector.crossProduct(-m_ViewDir), m_UpVector, -m_ViewDir,
				AcGePoint3d::kOrigin, AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
				AcGeVector3d::kZAxis));
	}*/

	// [KSA:TT2622] - fix begin
	bool bCalcUsrObjExtents = true;
	{
		RbCSViewportPtr Viewport( m_ViewportId, AcDb::kForRead );
		if ( Viewport.openStatus() == Acad::eOk )
		{
			RbCSDimStylePtr dimStyle( Viewport->GetDimStyleId(), AcDb::kForRead );
			if ( dimStyle.openStatus() == Acad::eOk )
				bCalcUsrObjExtents = dimStyle->getUserParts();
		}
	}
	if ( bCalcUsrObjExtents )
		getUserObjectsExtents(Extents);
	// [KSA:TT2622] - fix end

	if(m_IsClipped)
	{
		AcGePoint2d cornerA;
		AcGePoint2d cornerB;
		m_ClipBound.Get(cornerA, cornerB);

		AcGePoint3d minPoint3d(Extents.minPoint());
		AcGePoint3d maxPoint3d(Extents.maxPoint());

		minPoint3d.set(minPoint3d.x<cornerA.x? cornerA.x : minPoint3d.x,
			           minPoint3d.y<cornerA.y? cornerA.y : minPoint3d.y,
			           minPoint3d.z);

		maxPoint3d.set(maxPoint3d.x>cornerB.x? cornerB.x : maxPoint3d.x,
					   maxPoint3d.y>cornerB.y? cornerB.y : maxPoint3d.y,
					   maxPoint3d.z);

		Extents.set(minPoint3d, maxPoint3d);
	}
	myExtents.set( Extents.minPoint(), Extents.maxPoint() );
}

Acad::ErrorStatus RbCSDrawingPresenter::
subGetGeomExtents(AcDbExtents& Extents) const
{
	this->assertReadEnabled();

	if (m_ViewportId.isValid() == false)
		return Acad::eOk;

	this->getExtents(Extents);

	AcGePoint3d minPoint = Extents.minPoint();
	AcGePoint3d maxPoint = Extents.maxPoint();

	Extents.transformBy(m_OCS);

	if (m_SchemeId.isValid() == true)
	{
		RbCS2dSchemePresenterPtr Scheme(m_SchemeId, AcDb::kForRead);

		AcDbExtents SchemeExtents;

		if (Scheme->getGeomExtents(SchemeExtents) == Acad::eOk)
		{
			Extents.addExt(SchemeExtents);
		};
	}

	minPoint = Extents.minPoint();
	maxPoint = Extents.maxPoint();

	/* YD quick fix - wrong extents for viewports without geometry */
	/* START QUICK FIX*/

	if ( (maxPoint-minPoint).length() > 1e10 )
	{
		return Acad::eInvalidExtents;
	}

	/* END QUICK FIX */

	return Acad::eOk;
}
// fix bug 3012
Acad::ErrorStatus RbCSDrawingPresenter::
getGeomExtentsWithoutRotation(AcDbExtents& Extents) const
{
	this->assertReadEnabled();

	if (m_ViewportId.isValid() == false)
		return Acad::eOk;

	this->getExtents(Extents);

	AcGePoint3d minPoint = Extents.minPoint();
	AcGePoint3d maxPoint = Extents.maxPoint();

	AcGeMatrix3d rotMtrx( AcGeMatrix3d::kIdentity );
	{AcDbReader< RbCSViewport > pViewport( this->GetViewportId() );
	if ( pViewport.openStatus() == Acad::eOk ) {
		rotMtrx = AcGeMatrix3d::rotation( -pViewport->GetAngle(),	AcGeVector3d::kZAxis ); // remove rotation
	}
	}
	Extents.transformBy(m_OCS * rotMtrx);

	if (m_SchemeId.isValid() == true)
	{
		RbCS2dSchemePresenterPtr Scheme(m_SchemeId, AcDb::kForRead);

		AcDbExtents SchemeExtents;

		if (Scheme->getGeomExtents(SchemeExtents) == Acad::eOk)
		{
			Extents.addExt(SchemeExtents);
		};
	}

	minPoint = Extents.minPoint();
	maxPoint = Extents.maxPoint();

	/* YD quick fix - wrong extents for viewports without geometry */
	/* START QUICK FIX*/

	if ( (maxPoint-minPoint).length() > 1e10 )
	{
		return Acad::eInvalidExtents;
	}

	/* END QUICK FIX */

	return Acad::eOk;
}
// end 3012
#include <dbosnap.h>
//---------------------------------------------------------------------------------------------------------------
Acad::ErrorStatus RbCSDrawingPresenter::
subGetOsnapPoints(AcDb::OsnapMode OsnapMode, int GsSelectionMark,
		const AcGePoint3d& PickPoint, const AcGePoint3d& LastPoint,
		const AcGeMatrix3d& ViewXform, AcGePoint3dArray& SnapPoints,
		AcDbIntArray& GeomIds) const
{
#ifdef _DEBUG
__int64 counter;
::QueryPerformanceCounter((LARGE_INTEGER*) &counter);
__int64 frequency;
::QueryPerformanceFrequency((LARGE_INTEGER*) &frequency);
frequency /= 1000000;
#endif

	using namespace RbCS;
	this->assertReadEnabled();

	static int lastGsSelectionMarker = GsSelectionMark;

	static struct Cash
	{
		const RbCSDrawingPresenter* mOwner;
		CStateObserver::State mState;
		std::map< int, RbCS::CGeObject2d > mSnapObjects;
		RCAD::CGeDataCollection2 mColl;
		AcGeMatrix3d mTransf;
		AcGeMatrix3d mTrArc;

		Cash() : mOwner(0), mState(gPresenterStateObserver.GetState()) {}
		void SetCurPresenter( const RbCSDrawingPresenter* pres )
		{
			if ( pres->m_bNewbie || ( pres != mOwner ) )
			{
#ifdef _DEBUG
__int64 counter3;
::QueryPerformanceCounter((LARGE_INTEGER*) &counter3);
#endif	
				mColl.Clear();
				mColl = CGeDataAccessor::Instance()->GetDispGeSet( *pres, true, true );
				mSnapObjects.clear();
				mOwner = pres;
				mTransf = pres->GetOCS();
				/*
				if ((pres->Is3dView() == true) && (!RbCSDrawingPresenter::bUseIntImprove))
				{
					AcGeMatrix3d transf = AcGeMatrix3d::alignCoordSys(
						AcGePoint3d::kOrigin, pres->GetUpVector().crossProduct(-pres->GetViewDir()),
						pres->GetUpVector(), -pres->GetViewDir(), AcGePoint3d::kOrigin,
						AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
						AcGeVector3d::kZAxis);
					mTransf *= transf;
				}
				*/
				mTrArc = mTransf;
				/*
				AcDbObjectPointer< RbCSViewport > Viewport( pres->GetViewportId(), AcDb::kForRead );
				if ( Viewport.openStatus() == Acad::eOk )
				{
					if (( pres->Is3dView() != true ) &&(!RbCSDrawingPresenter::bUseIntImprove))
					{
						AcGeMatrix3d tmpMatr;
						AcGeVector3d vDir, uDir;
						Viewport->getProjComponents( tmpMatr, vDir, uDir );
						tmpMatr.entry[0][3] = 0.0;
						tmpMatr.entry[1][3] = 0.0;
						tmpMatr.entry[2][3] = 0.0;
						mTrArc = mTrArc.postMultBy( tmpMatr );
					}
				}
				*/

#ifdef _DEBUG
__int64 counter4;
::QueryPerformanceCounter((LARGE_INTEGER*) &counter4);
__int64 frequency;
::QueryPerformanceFrequency((LARGE_INTEGER*) &frequency);
frequency /= 1000000;
time_explodes += (counter4 - counter3) / frequency;
num_explodes++;
#endif
			}
		}

		const RbCS::CGeObject2d* AddObject( const RbCSDrawingPresenter* pres, int mark, const RbCS::CGeObject2d& obj )
		{
			SetCurPresenter( pres );
			return &( *( mSnapObjects.insert( std::make_pair( mark, obj ) ).first ) ).second;
		}
		const RbCS::CGeObject2d* GetGeObject( const RbCSDrawingPresenter* pres, int mark )
		{
			const RbCS::CGeObject2d* r = 0;

			if ( gPresenterStateObserver.GetState() != mState )
			{
				mOwner = 0;										// Reset presenter when its state changed 
				mState = gPresenterStateObserver.GetState();	// (to handle shortenings or any other visible changes)
			}

			if ( pres == mOwner )
			{
				std::map< int, RbCS::CGeObject2d >::iterator it;
				it = mSnapObjects.find( mark );	// search in cash
				if ( it != mSnapObjects.end() )
					r = &(*it).second;
				else
				{
					// extract GeObject from collection and cash it
					RCAD::CGeDataCollection2::ReaderPtr rd = mColl.GetReader();
					r = AddObject( pres, mark, (*rd)[ mark ] );
				}
			}
			return r;
		}
	} cash;

	if ( lastGsSelectionMarker != GsSelectionMark || this != cash.mOwner ||
		gPresenterStateObserver.GetState() != cash.mState )
	{
		delete m_pEntity;
		m_pEntity = NULL;
		lastGsSelectionMarker = GsSelectionMark;
		cash.SetCurPresenter( this );
	}

	const CGeObject2d* pcObj = 0;
	
	if ( m_bNewbie == false ) 
	{
		pcObj = cash.GetGeObject( this, GsSelectionMark );
	}


	if ( m_pEntity == NULL )
	{
		if ( m_minGSArcMarker != 0 && GsSelectionMark < m_minGSArcMarker )
		{
			pcObj = cash.GetGeObject( this, GsSelectionMark );
			if ( pcObj == 0 )
			{
				cash.SetCurPresenter( this );
				pcObj = cash.GetGeObject( this, GsSelectionMark );
	 	                m_bNewbie = false;
			}
			
			if ( pcObj != 0 )
			{
				const CGeObject2d& geObject = *pcObj;

				switch ( geObject.GetGeType() )
				{
				case AcGe::kLineSeg2d:
				case AcGe::kLineSeg3d:
					{
						AcGePoint3d s = AcGePoint3d( AcGePlane::kXYPlane, geObject.GetPoints().first );
						AcGePoint3d e = AcGePoint3d( AcGePlane::kXYPlane, geObject.GetPoints().second );

						m_pEntity = new AcDbLine( 
							s.transformBy( cash.mTransf ), 
							e.transformBy( cash.mTransf ) );

						assert( m_pEntity != 0 );

						break;
					}
				/*case AcGe::kCircArc2d:
					{
						AcGePoint3d center = geObject.GetPoints().first;

						AcGeCircArc3d circ( 
							AcGePoint3d( center.x, center.y, 0.0 ),
							-AcGeVector3d::kZAxis,
							center.z );
						circ.transformBy( cash.mTransf );

						m_pEntity = new AcDbCircle(
							circ.center(),
							circ.normal(),
							circ.radius() ); 
						assert( m_pEntity != 0 );
					}
					break;
				case AcGe::kCircArc3d:
					{
						std::auto_ptr< AcGeEntity3d > e3d = geObject.AsEntity3d();
						AcGeCircArc3d* pCircle3d = static_cast< AcGeCircArc3d* >( e3d.get() );
						assert( pCircle3d != 0 );

						pCircle3d->transformBy( cash.mTransf );

						m_pEntity = new AcDbCircle( pCircle3d->center(),
							pCircle3d->normal(),
							pCircle3d->radius() ); 
						assert( m_pEntity != 0 );
					}
					break;
				*/
				default:
					assert( false );
				}
			}
		}
		else
		{
#ifdef _DEBUG
__int64 counter5;
::QueryPerformanceCounter((LARGE_INTEGER*) &counter5);
#endif
			RCAD::CGeArcCollection arc_set = GetOrigGeArcSet();
			GetDispGeArcSet( arc_set, true, true );
#ifdef _DEBUG
__int64 counter6;
::QueryPerformanceCounter((LARGE_INTEGER*) &counter6);
time_explArcs += (counter6 - counter5) / frequency;
num_explArcs++;
#endif
			int pos = GsSelectionMark - m_minGSArcMarker;
			if ( pos < arc_set.size() )
			{
				const RbCS::CGeArc& geObject = arc_set[ pos ];
				if ( geObject.IsKindOf( AcGe::kCircArc3d ) )
				{
					RbCS::TEntity3dPtr ent = geObject.AsEntity3d();
					AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >( ent.get() );

					if ( !arc->isClosed() )
					{
						double stAngle = arc->startAng();
						double enAngle = arc->endAng();
						AcGePoint3d center = arc->center();
						AcGeVector3d normal = arc->normal();
						double radius = arc->radius();
						AcDbArc* newArc = new AcDbArc(
							center,
							normal,
							radius,
							stAngle,
							enAngle );
						assert( newArc != 0 );

						AcGePoint3d arcStPnt = arc->startPoint();
						AcGePoint3d arcDbStPnt;
						newArc->getStartPoint( arcDbStPnt );

						AcGePoint3d arcEnPnt = arc->endPoint();
						AcGePoint3d arcDbEnPnt;
						newArc->getEndPoint( arcDbEnPnt );

						AcGeVector3d arcVectSt = arcStPnt - center;
						AcGeVector3d arcDbVectSt = arcDbStPnt - center;

						AcGePlane plane( center, normal );
						AcGeVector2d stVect2d = arcVectSt.convert2d( plane );
						AcGeVector2d stDbVect2d = arcDbVectSt.convert2d( plane );
						double angle = stVect2d.angleTo( stDbVect2d );
						angle = stVect2d.perpVector().dotProduct( stDbVect2d ) > 0 ? angle : M_2PI - angle;

						AcGeMatrix3d rotMtrx = AcGeMatrix3d::rotation( -angle, normal, center );
						newArc->transformBy( rotMtrx );

						m_pEntity = newArc;
					}
					else
					{
						m_pEntity = new AcDbCircle(
							arc->center(),
							arc->normal(),
							arc->radius());
						assert( m_pEntity != 0 );
					}
				}
				else
				{
					assert( false );
				}
				if ( m_pEntity != 0 )
				{
					m_pEntity->transformBy( cash.mTrArc );
				}
			}

		}
	}

	Acad::ErrorStatus es = Acad::eNotApplicable;
	if ( m_pEntity != NULL )
	{
		es = m_pEntity->getOsnapPoints( OsnapMode, GsSelectionMark,
			PickPoint, LastPoint, ViewXform, SnapPoints, GeomIds );
	}

#ifdef _DEBUG
	__int64 counter2;
	::QueryPerformanceCounter((LARGE_INTEGER*) &counter2);
	time_osnaps += (counter2 - counter) / frequency;
	num_osnaps++;
#endif

	return es;
}

//---------------------------------------------------------------------------------------------------------------
Adesk::UInt32 RbCSDrawingPresenter::
subSetAttributes(AcGiDrawableTraits* drawableTraits)
{
	assert(drawableTraits != 0);

	Adesk::UInt32 result =
			this->RbCSDrawingEntity::subSetAttributes(drawableTraits);

	return result | kDrawableViewIndependentViewportDraw;
}

Acad::ErrorStatus RbCSDrawingPresenter::
subTransformBy(const AcGeMatrix3d& XForm)
{
	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		UndoFiler->writeItem((long)desc());
		UndoFiler->writeItem((long)uocTransformBy);

		AcGeDwgIO::outFields(UndoFiler, XForm);
	}

	m_OCS.preMultBy(XForm);

	RCAD::TSoftPtrIdVector::iterator Iter = m_WeldSeams.begin();
	RCAD::TSoftPtrIdVector::iterator End = m_WeldSeams.end();

	for (; Iter != End; ++Iter)
	{
		AcDbEntityPointer Seam(*Iter, AcDb::kForWrite);

		Seam->transformBy(XForm);
	}

	if (this->isNewObject() == Adesk::kFalse)
	{
		this->updateHatch();

		this->updateDwgUserObjEntities();
	}

	return Acad::eOk;
}

void RbCSDrawingPresenter::
resolveHolesPrimitivesVisibility( RCAD::CGeDataCollection2& primitives ) const
{
	using namespace RbCS;
	bool showHoles = true;
	bool showHiddenHoles = false;

	AcDbObjectPointer<RbCSViewport> Viewport(this->ViewportId, AcDb::kForRead);
	if (Viewport.openStatus() == Acad::eOk)
	{
		RbCSDimStylePtr pStyle( Viewport->GetDimStyleId(), AcDb::kForRead );
		if( pStyle.openStatus() == Acad::eOk )
		{
			RBCTDrawing::eViewType iType = Viewport->GetViewType();
			AcDbObjectId viewId = pStyle->GetViewDimStyleId( RbCSDimStyle::normalizeViewType( iType ) );
			RbCSViewDimStylePtr pView( viewId, AcDb::kForRead );
			if( pView.openStatus() == Acad::eOk )
			{
				showHoles = pView->getIsDescription( RBCSDimStyle::dcHolesBolts );
				showHiddenHoles = pView->getIsDescription( RBCSDimStyle::dcHiddenHoles );

				if (showHoles == true)
				{
					AcDbObjectId descId = pView->GetDescStyleId( RBCSDimStyle::dcHolesBolts );
					RbCSHolesBoltsDescStylePtr pHolesDesc( descId, AcDb::kForRead );
					if( pHolesDesc.openStatus() == Acad::eOk )
					{
						RbCSHolesBoltsDescStyle::eSight iSight = pHolesDesc->getSight();
						if( iSight != RbCSHolesBoltsDescStyle::sReal &&
							iSight != RbCSHolesBoltsDescStyle::sRealAxes )
							showHoles = false;
					}
				}

				if (showHiddenHoles == true)
				{
					AcDbObjectId descId = pView->GetDescStyleId( RBCSDimStyle::dcHiddenHoles );
					RbCSHolesBoltsDescStylePtr pHHDesc( descId, AcDb::kForRead );
					if( pHHDesc.openStatus() == Acad::eOk )
					{
						RbCSHolesBoltsDescStyle::eSight iSight = pHHDesc->getSight();
						if( iSight != RbCSHolesBoltsDescStyle::sReal &&
							iSight != RbCSHolesBoltsDescStyle::sRealAxes )
							showHiddenHoles = false;
					}
				}
			}
		}
	}

	if (this->Is3dView() == true)
	{
		showHoles = true;
		showHiddenHoles = true;
	}

	RCAD::CGeDataCollection2::WriterPtr wr = primitives.GetWriter();
	size_t size = wr->GetSize();
	for (unsigned i = 0; i < size; ++i)
	{
		CGeObject2d& obj = (*wr)[i];

		if ( ( obj.GetTag() == 1 ) || ( ( obj.GetTag() == 2 ) ) )
		{
			if ( obj.IsHidden() == false )
			{
				obj.SetVisible( showHoles );
			}
			else
			{
				obj.SetVisible( showHiddenHoles );
			}
		}
	}
	RCAD::CGeArcCollection::iterator iter_arc = m_circs.begin();
	RCAD::CGeArcCollection::iterator iter_arc_end = m_circs.end();
	for ( ; iter_arc != iter_arc_end; ++iter_arc )
	{
		if ( iter_arc->GetTag() == 1 || iter_arc->GetTag() == 2 )
		{
			if ( iter_arc->IsHidden() == false )
			{
				iter_arc->SetVisible( showHoles );
			}
			else
			{
				iter_arc->SetVisible( showHiddenHoles );
			}
		}
	}
}

void RbCSDrawingPresenter::
subViewportDraw(AcGiViewportDraw* DrawCntx)
{
	//CMemoryMeterTool mmt( _T("RbCSDrawingPresenter::viewportDraw") );

	using namespace RbCS;
	assert(DrawCntx != 0);

	if ( IsValid() == false )
	{
		// don't try to draw invalid viewports
		return;
	}

	AcDbObjectPointer<AcDbLayerTableRecord> pLayer(this->layerId(),
			AcDb::kForRead);
	if (pLayer.openStatus() == Acad::eOk)
	{
		if (pLayer->isOff() || pLayer->isFrozen())
			return;
	}

	if (DrawCntx->regenAbort() == Adesk::kFalse)
	{
		CLineInfo LineInfo;

		AcDbObjectId cutBrLineTypeId;
		Adesk::UInt16 cutBrLineColour = 0;
		double cutBrLineExt = 0.0;

		// fix bug TT#2933
		bool showUserParts = true;
		AcDbObjectId userPartsLineTypeId;
		Adesk::UInt16 userPartsLineColour = 0;
		AcDb::LineWeight userPartsLineWeight = AcDb::kLnWt000;
		// end TT#2933

		// Painted Machining
		bool machPresentation = false;
		AcDbObjectId machPartsLineTypeId;
		Adesk::UInt16 machPartsLineColour = 0;
		AcDb::LineWeight machPartsLineWeight = AcDb::kLnWt000;
		//

		{
			AcDbObjectPointer<RbCSViewport> Viewport(m_ViewportId,
					AcDb::kForRead);
			if (Viewport.openStatus() != Acad::eOk)
				return;

			Viewport->GetHiddenLineInfo(LineInfo);

			{RbCSDimStylePtr dimStyle(Viewport->GetDimStyleId(), AcDb::kForRead);
			if (dimStyle.openStatus() == Acad::eOk)
			{
				cutBrLineTypeId = dimStyle->getCutBreakLineId();
				cutBrLineColour = dimStyle->getCutLineColour();
				cutBrLineExt = dimStyle->GetAxesExtention() /
						(Viewport->GetScale().GetScale() * 2.0);

				showUserParts = dimStyle->getUserParts();
				userPartsLineTypeId = dimStyle->getUserPartsLineTypeId();
				userPartsLineColour = dimStyle->getUserPartsLineColour();
				userPartsLineWeight = dimStyle->getUserPartsLineThickness();
			}}
			{RbCSSinglePartDimStylePtr dimStyleSP( Viewport->GetDimStyleId(), AcDb::kForRead );
			if ( dimStyleSP.openStatus() == Acad::eOk )
			{
				machPresentation = dimStyleSP->getMachiningPresentation() == RbCSSinglePartDimStyle::mpDistinct;
				machPartsLineTypeId = dimStyleSP->getMachiningLineType();
				machPartsLineColour = dimStyleSP->getMachiningLineColour();
				machPartsLineWeight = dimStyleSP->getMachiningLineWeight();
			}}
		}

		AcGiViewport* Viewport = &(DrawCntx->viewport());
		assert(Viewport != 0);

		AcGiViewportGeometry* ViewportGeometry = &(DrawCntx->geometry());
		assert(ViewportGeometry != 0);

		AcGiGeometry* Geometry = DrawCntx->rawGeometry();
		assert(Geometry != 0);

		AcGiSubEntityTraits* SubEntTraits = &(DrawCntx->subEntityTraits());
		assert(SubEntTraits != 0);

		AcGeMatrix3d transf = m_OCS;

		/*
		if ((this->Is3dView() == true) && (!RbCSDrawingPresenter::bUseIntImprove))
		{
			AcGeMatrix3d Transf = AcGeMatrix3d::alignCoordSys(
					AcGePoint3d::kOrigin, m_UpVector.crossProduct(-m_ViewDir),
					m_UpVector,	-m_ViewDir, AcGePoint3d::kOrigin,
					AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
					AcGeVector3d::kZAxis);
			transf *= Transf;
		}
		*/

		RCAD::CGeDataCollection2 displayed_set = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );

		this->resolveHolesPrimitivesVisibility(displayed_set);

		RCAD::CGeDataCollection2::ReaderPtr rd = displayed_set.GetReader();	
		size_t size = rd->GetSize();
		m_minGSArcMarker = size;
		unsigned i;
		for ( i = 0; i < size; ++i)
		{
			SubEntTraits->setSelectionMarker(i);

			const CGeObject2d& obj = (*rd)[i];

			if ( obj.IsVisible() == false )
				continue;

			if ( obj.IsHidden() == true )
			{
				if ( ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj ) &&
					( showUserParts == false ) )
					continue;

				if ( LineInfo.m_bVisible == false )
					continue;

				SubEntTraits->setColor(LineInfo.m_iColorIndex);
				SubEntTraits->setLineType(LineInfo.m_LineTypeId);
				SubEntTraits->setLineTypeScale(LineInfo.m_dLineTypeScale);
				SubEntTraits->setLineWeight(LineInfo.m_iLineWeight);
			}
			else
			{
				if ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj )
				{
					if (showUserParts == false)
						continue;

					SubEntTraits->setColor(userPartsLineColour);
					SubEntTraits->setLineType(userPartsLineTypeId);
					SubEntTraits->setLineTypeScale(this->linetypeScale());
					SubEntTraits->setLineWeight(userPartsLineWeight);
				}
				else if ( machPresentation && ( obj.GetTag() == RCAD::stDrillingStraight ||
					obj.GetTag() == RCAD::stDrillingCircle || obj.GetTag() == RCAD::stMachining ) )
				{
					SubEntTraits->setColor( machPartsLineColour );
					SubEntTraits->setLineType( machPartsLineTypeId );
					SubEntTraits->setLineTypeScale( this->linetypeScale() );
					SubEntTraits->setLineWeight( machPartsLineWeight );
				}
				else
				{
					SubEntTraits->setColor(this->colorIndex());
					SubEntTraits->setLineType(this->linetypeId());
					SubEntTraits->setLineTypeScale(this->linetypeScale());
					SubEntTraits->setLineWeight(this->lineWeight());
				}
			}

			double segmentExt = 0.0;

			if ( obj.GetTag() == RCAD::stBreakLine )
			{
				SubEntTraits->setColor(cutBrLineColour);
				SubEntTraits->setLineType(cutBrLineTypeId);
				segmentExt = cutBrLineExt;
			}

			SubEntTraits->setSelectionMarker(i);

			switch ( obj.GetGeType() )
			{
				/*case AcGe::kLineSeg3d:
				{
					TEntity3dPtr e3d = obj.AsEntity3d();
					const AcGeLineSeg3d* LineSeg =
							static_cast<const AcGeLineSeg3d*>( e3d.get() );
					assert(LineSeg != 0);

					AcGeVector3d dir = LineSeg->direction();

					AcGePoint3d Points[2];
					Points[0] = LineSeg->startPoint() - dir * segmentExt;
					Points[1] = LineSeg->endPoint() + dir * segmentExt;

					Points[0].transformBy(transf);
					Points[1].transformBy(transf);

					Geometry->polyline(2, Points);

					break;
				}*/

				case AcGe::kLineSeg2d:
				{
					TEntity2dPtr e2d = obj.AsEntity2d();
					const AcGeLineSeg2d* LineSeg =
							static_cast<const AcGeLineSeg2d*>( e2d.get() );
					assert(LineSeg != 0);

					AcGeVector2d dir = LineSeg->direction();

					AcGePoint2d StartPoint = LineSeg->startPoint() -
							dir * segmentExt;
					AcGePoint2d EndPoint = LineSeg->endPoint() + dir * segmentExt;

					AcGePoint3d Points[2];
					Points[0] = AcGePoint3d(StartPoint.x, StartPoint.y, 0.0);
					Points[1] = AcGePoint3d(EndPoint.x, EndPoint.y, 0.0);

					Points[0].transformBy(transf);
					Points[1].transformBy(transf);

					Geometry->polyline(2, Points);

					break;
				}/*
				case AcGe::kCircArc2d:
				{

					AcGePoint3d center = obj.GetPoints().first;

					AcGeCircArc3d circ( 
						AcGePoint3d( center.x, center.y, 0.0 ),
						AcGeVector3d::kZAxis,
						center.z );

					circ.transformBy( transf );

					Geometry->circle( circ.center(),
						circ.radius(),
						circ.normal() );
					break;
				}
				case AcGe::kCircArc3d:
				{
					TEntity3dPtr e3d = obj.AsEntity3d();
					AcGeCircArc3d* pCircle3d = static_cast< AcGeCircArc3d* >( e3d.get() );
					assert( pCircle3d != 0 );

					pCircle3d->transformBy( transf );

					Geometry->circle( pCircle3d->center(),
						pCircle3d->radius(),
						pCircle3d->normal() );
					break;
				}*/

				default:
					assert(0);
			}
		}
		
		/*{
			AcDbObjectPointer<RbCSViewport> pViewport(m_ViewportId,
					AcDb::kForRead);
			if (pViewport.openStatus() != Acad::eOk)
				return;

			RbCSDimStylePtr pDimStyle(pViewport->GetDimStyleId(),
					AcDb::kForRead);
			if (pDimStyle.openStatus() == Acad::eOk)
			{
				SubEntTraits->setColor(pDimStyle->getUserPartsLineColour());
				SubEntTraits->setLineType(pDimStyle->getUserPartsLineTypeId());
				SubEntTraits->setLineWeight(
						pDimStyle->getUserPartsLineThickness());
			}
		}

		this->drawUserObjects(DrawCntx);*/

		{AcDbObjectPointer< RbCSViewport > Viewport( m_ViewportId, AcDb::kForRead );
		if ( Viewport.openStatus() != Acad::eOk )
		{
			return;
		}
		AcGeMatrix3d mtrx = transf;
		/*
		if ((this->Is3dView() != true ) && (!RbCSDrawingPresenter::bUseIntImprove))
		{
			AcGeMatrix3d tmpMatr;
			AcGeVector3d vDir, uDir;
			Viewport->getProjComponents( tmpMatr, vDir, uDir );
			tmpMatr.entry[0][3] = 0.0;
			tmpMatr.entry[1][3] = 0.0;
			tmpMatr.entry[2][3] = 0.0;
			mtrx = mtrx.postMultBy( tmpMatr );
		}
		*/
		RCAD::CGeArcCollection arc_set = GetOrigGeArcSet();
		GetDispGeArcSet( arc_set, true, true );

		RbCS::GeArcsArray::iterator it_circ = arc_set.begin();
		RbCS::GeArcsArray::iterator it_circ_end = arc_set.end();
		i--;
		for ( ; it_circ != it_circ_end; ++it_circ )
		{
			i++;
			SubEntTraits->setSelectionMarker( i );
			//
			const CGeArc& obj = *it_circ;

			if ( obj.IsVisible() == false )
				continue;

			if ( obj.IsHidden() == true )
			{
				if ( ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj ) &&
					( showUserParts == false ) )
					continue;

				if ( LineInfo.m_bVisible == false )
					continue;

				SubEntTraits->setColor(LineInfo.m_iColorIndex);
				SubEntTraits->setLineType(LineInfo.m_LineTypeId);
				SubEntTraits->setLineTypeScale(LineInfo.m_dLineTypeScale);
				SubEntTraits->setLineWeight(LineInfo.m_iLineWeight);
			}
			else
			{
				if ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj )
				{
					if (showUserParts == false)
						continue;

					SubEntTraits->setColor(userPartsLineColour);
					SubEntTraits->setLineType(userPartsLineTypeId);
					SubEntTraits->setLineTypeScale(this->linetypeScale());
					SubEntTraits->setLineWeight(userPartsLineWeight);
				}
				else if ( machPresentation && ( obj.GetTag() == RCAD::stDrillingStraight ||
					obj.GetTag() == RCAD::stDrillingCircle || obj.GetTag() == RCAD::stMachining ) )
				{
					SubEntTraits->setColor( machPartsLineColour );
					SubEntTraits->setLineType( machPartsLineTypeId );
					SubEntTraits->setLineTypeScale( this->linetypeScale() );
					SubEntTraits->setLineWeight( machPartsLineWeight );
				}
				else
				{
					SubEntTraits->setColor(this->colorIndex());
					SubEntTraits->setLineType(this->linetypeId());
					SubEntTraits->setLineTypeScale(this->linetypeScale());
					SubEntTraits->setLineWeight(this->lineWeight());
				}
			}
			//

			if ( it_circ->IsKindOf( AcGe::kCircArc3d ) )
			{
				TEntity3dPtr ent = it_circ->AsEntity3d();
				AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >( ent.get() );
				arc->transformBy( mtrx );
				if ( !arc->isClosed() )
				{
					AcGePoint3d stPoint, enPoint, cnPoint;;
					stPoint = arc->startPoint();
					enPoint = arc->endPoint();
					AcGeInterval iterv;
					arc->getInterval( iterv );
					double intLength = iterv.length();
					cnPoint = arc->evalPoint( intLength / 2 );
					Geometry->circularArc( stPoint, cnPoint, enPoint );
				}
				else
				{
					Geometry->circle( arc->center(), arc->radius(), arc->normal() );
				}
			}
		}}
	}
}

void RbCSDrawingPresenter::
drawUserObjects(AcGiViewportDraw* pDrawCntx)
{
	assert(pDrawCntx != 0);

	pDrawCntx->geometry().pushModelTransform(m_OCS);

	UserObjEntries::const_iterator iter = myUserObjEntries.begin();
	UserObjEntries::const_iterator iterEnd = myUserObjEntries.end();
	for (; iter != iterEnd; ++iter)
	{
		this->drawUserObject(pDrawCntx, *iter);
	}

	pDrawCntx->geometry().popModelTransform();
}

#include "../rbcsroot/douserobj.h"

void RbCSDrawingPresenter::
getUserObjViewInfo(const UserObjEntry& entry, AcDbObjectId& blockId,
		AcGeScale3d& scale, AcGeMatrix3d& transform) const
{
	assert(entry.myObjectId.isValid() == true);

	AcGeVector3d viewDir = m_ViewDir;
	AcGeVector3d upDir = m_UpVector;

	RbCSDOUserObjPtr pUserObject(entry.myObjectId, AcDb::kForRead);

	if (pUserObject.openStatus() == Acad::eOk)
	{
		AcGePoint3d objO;
		AcGeVector3d objX = AcGeVector3d::kXAxis;
		AcGeVector3d objY = AcGeVector3d::kYAxis;

		entry.myPlane.getCoordSystem(objO, objX, objY);

		AcGeVector3d objZ = objX.crossProduct(objY);

		AcGeMatrix3d objCS;
		objCS.setCoordSystem(objO, objX, objY, objZ);

		AcGeScale3d objScale = pUserObject->GetScale();

		AcGePlane viewPlane(AcGePoint3d::kOrigin, viewDir.crossProduct(upDir),
				upDir);

		if (viewDir.isParallelTo(objX) == Adesk::kTrue)
		{
			blockId = pUserObject->GetBlockIdYZ();

			scale.set(objScale.sy, objScale.sz, 1.0);

			AcGePoint3d objViewO(AcGePlane::kXYPlane, objO.convert2d(viewPlane));
			AcGeVector3d objViewX(AcGePlane::kXYPlane, objY.convert2d(viewPlane));

			if (viewDir.isCodirectionalTo(objX) == Adesk::kTrue)
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, - AcGeVector3d::kZAxis), - AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector()) *
						AcGeMatrix3d::rotation(M_PI, AcGeVector3d::kXAxis);
			}
			else
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, AcGeVector3d::kZAxis), AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector());
			}
		}
		else if (viewDir.isParallelTo(objY) == Adesk::kTrue)
		{
			blockId = pUserObject->GetBlockIdXZ();

			scale.set(objScale.sx, objScale.sz, 1.0);

			AcGePoint3d objViewO(AcGePlane::kXYPlane, objO.convert2d(viewPlane));
			AcGeVector3d objViewX(AcGePlane::kXYPlane, objX.convert2d(viewPlane));

			if (viewDir.isCodirectionalTo(objY) == Adesk::kTrue)
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, - AcGeVector3d::kZAxis), - AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector()) *
						AcGeMatrix3d::rotation(M_PI, AcGeVector3d::kXAxis) *
						AcGeMatrix3d::rotation(M_PI, AcGeVector3d::kXAxis);
			}
			else
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, AcGeVector3d::kZAxis), AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector()) *
						AcGeMatrix3d::rotation(M_PI, AcGeVector3d::kXAxis);
			}
		}
		else if (viewDir.isParallelTo(objZ) == Adesk::kTrue)
		{
			blockId = pUserObject->GetBlockIdXY();

			scale.set(objScale.sx, objScale.sy, 1.0);

			AcGePoint3d objViewO(AcGePlane::kXYPlane, objO.convert2d(viewPlane));
			AcGeVector3d objViewX(AcGePlane::kXYPlane, objX.convert2d(viewPlane));

			if (viewDir.isCodirectionalTo(objZ) == Adesk::kTrue)
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, - AcGeVector3d::kZAxis), - AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector()) *
						AcGeMatrix3d::rotation(M_PI, AcGeVector3d::kXAxis);
			}
			else
			{
				transform = AcGeMatrix3d::rotation(AcGeVector3d::kXAxis.angleTo(
						objViewX, AcGeVector3d::kZAxis), AcGeVector3d::kZAxis,
						objViewO) * AcGeMatrix3d::translation(objViewO.asVector());
			}
		}
		else
		{
			blockId = pUserObject->GetBlockId3D();

			scale = objScale;

			transform = AcGeMatrix3d::alignCoordSys(AcGePoint3d::kOrigin,
					viewDir.crossProduct(-viewDir), upDir, -viewDir,
					AcGePoint3d::kOrigin, AcGeVector3d::kXAxis,
					AcGeVector3d::kYAxis, AcGeVector3d::kZAxis);

			transform.postMultBy(objCS);
		}
	}
}

void RbCSDrawingPresenter::
drawUserObject(AcGiViewportDraw* pDrawCntx, const UserObjEntry& entry)
{
	assert(pDrawCntx != 0);
	assert(entry.myObjectId.isValid() == true);

	AcDbObjectId blockId;
	AcGeScale3d scale;
	AcGeMatrix3d transform;
	this->getUserObjViewInfo(entry, blockId, scale, transform);

	if (blockId.isNull() == false)
	{
		AcDbBlockTableRecordPointer pBlock(blockId, AcDb::kForRead);

		if (pBlock.openStatus() == Acad::eOk)
		{
			pDrawCntx->geometry().pushModelTransform(transform);
			pDrawCntx->geometry().pushModelTransform(scale);

			pDrawCntx->geometry().draw(pBlock.object());

			pDrawCntx->geometry().popModelTransform();
			pDrawCntx->geometry().popModelTransform();
		}
	}
}

void RbCSDrawingPresenter::
getUserObjectsExtents(AcDbExtents& extents) const
{
	AcGePoint3d minPoint = extents.minPoint();
	AcGePoint3d maxPoint = extents.maxPoint();

	AcDbExtents usrObjExtents; // [KSA:TT2622] - added

	UserObjEntIds::const_iterator iter = myDwgUserObjEntIds.begin();
	UserObjEntIds::const_iterator iterEnd = myDwgUserObjEntIds.end();
	for (; iter != iterEnd; ++iter)
	{
		DwgUserObjEntityPtr pDwgUserObjEntity(*iter, AcDb::kForRead);

		if (pDwgUserObjEntity.openStatus() == Acad::eOk)
		{
			Acad::ErrorStatus es = pDwgUserObjEntity->getGeomExtents( usrObjExtents );
			assert(es == Acad::eOk);
		}

		minPoint = usrObjExtents.minPoint();
		maxPoint = usrObjExtents.maxPoint();
	}

	// [KSA:TT2622] - fix begin
	if ( usrObjExtents.minPoint().x <= usrObjExtents.maxPoint().x ) {
		AcGePoint2dArray points;
		AcArray<bool> hiddenFlags;

		minPoint = usrObjExtents.minPoint();
		maxPoint = usrObjExtents.maxPoint();

		points.append( AcGePoint2d( minPoint.x, minPoint.y ) );
		points.append( AcGePoint2d( maxPoint.x, maxPoint.y ) );
		ToShortedLocation( points, hiddenFlags );

		for ( size_t i = 0; i < points.length(); ++i ) {
			if ( hiddenFlags[i] == false ) {
				AcGePoint2d& pt = points[i];
				extents.addPoint( AcGePoint3d( pt.x, pt.y, 0 ) );
			}
		}
	}
	// [KSA:TT2622] - fix end
}

Adesk::Boolean RbCSDrawingPresenter::
subWorldDraw(AcGiWorldDraw* pDrawCntx)
{
	assert(pDrawCntx != 0);

	if (m_uReadedVersion == 8)
	{
		this->convertUserObjEntries();
	}

	if (pDrawCntx->regenAbort() == Adesk::kTrue)
	{
		return Adesk::kTrue;
	}

	return Adesk::kFalse;
}

void RbCSDrawingPresenter::
xmitPropagateModify() const
{
	RbCSDrawingEntity::xmitPropagateModify();

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; Iter++)
	{
		TDwgShorteningPtr Shortening(*Iter, AcDb::kForWrite);
		if (Shortening.openStatus() == Acad::eOk)
		{
			Shortening->recordGraphicsModified();
		}
	}

	RCAD::TSoftPtrIdVector::const_iterator iter = m_WeldSeams.begin();
	RCAD::TSoftPtrIdVector::const_iterator iterEnd = m_WeldSeams.end();
	for (; iter != iterEnd; ++iter)
	{
		RbCSWeldSeamPtr seam(*iter, AcDb::kForWrite);
		if (seam.openStatus() == Acad::eOk)
		{
			seam->recordGraphicsModified();
		}
	}

	{
		AcDbEditor< RbCSViewport > viewport( this->GetViewportId() );
		if ( viewport )
			viewport->notify( RbCSViewport::taMarkers );
	}

	UserObjEntIds::const_iterator uoIter = myDwgUserObjEntIds.begin();
	UserObjEntIds::const_iterator uoIterEnd = myDwgUserObjEntIds.end();
	for (; uoIter != uoIterEnd; ++uoIter)
	{
		DwgUserObjEntityPtr pDwgUserObjEntity(*uoIter, AcDb::kForWrite);

		if (pDwgUserObjEntity.openStatus() == Acad::eOk)
		{
			pDwgUserObjEntity->recordGraphicsModified();
		}
	}
}

bool RbCSDrawingPresenter::
Create(const AcDbObjectId& ObjectId, const AcDbObjectId& ViewportId)
{
	assert(ObjectId.isValid() == true);
	assert(ViewportId.isValid() == true);

	bool RetVal = false;

	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		UndoFiler->writeItem((long)desc());
		UndoFiler->writeItem((long)uocCreate);
	}

	this->m_ObjectId = ObjectId;
	this->m_ViewportId = ViewportId;

	AcDbObjectPointer<RbCSViewport> Viewport(ViewportId, AcDb::kForRead);

	AcDbDatabase* Database = Viewport->database();
	assert(Database != 0);

	AcDbBlockTable* BlockTable = 0;
	AcDbBlockTableRecord* ModelSpaceRecord = 0;

	Acad::ErrorStatus es = Database->getBlockTable(BlockTable, AcDb::kForRead);
	if (es == Acad::eOk)
	{
		assert(BlockTable != 0);

		es = BlockTable->getAt(ACDB_MODEL_SPACE, ModelSpaceRecord,
				AcDb::kForWrite);
		if (es == Acad::eOk)
		{
			assert(ModelSpaceRecord != 0);

			AcDbObjectId Id = AcDbObjectId::kNull;

			es = ModelSpaceRecord->appendAcDbEntity(Id, this);

			if (es == Acad::eOk)
			{
				RetVal = true;
			}

			ModelSpaceRecord->close();
		}

		BlockTable->close();
	}

	return RetVal;
}

void RbCSDrawingPresenter::
AddLineSeg( const RbCS::CGeObject& LineSeg )
{
	if ( m_WR.get() == 0 )
	{
		m_WR = m_aGeObjects.GetWriter();
	}

	m_WR->Append() = LineSeg;
}

/*
unsigned long RbCSDrawingPresenter::
AddLineSegTo(const AcGePoint3d& DstPoint, int ColorIndex, bool IsHidden,
		const AcDbObjectId& LinkedObjectId, unsigned long Tag,
		const AcGePoint3d& NodePoint, unsigned int machiningId)
{
	AcGePoint3d SrcPoint = DstPoint;

	if (m_aGeObjects.GetSize() != 0)
	{
		RCAD::TStdGeObject GeObject = m_aGeObjects.Last();
		const AcGeEntity3d* GeEntity = GeObject.AsEntity3d();

		if (GeEntity != 0)
		{
			if (GeEntity->isKindOf(AcGe::kCurve3d) == Adesk::kTrue)
			{
				const AcGeCurve3d* Curve =
						static_cast<const AcGeCurve3d*>(GeEntity);
				assert(Curve != 0);

				AcGePoint3d EndPoint = AcGePoint3d::kOrigin;
				if (Curve->hasEndPoint(EndPoint) == Adesk::kTrue)
				{
					SrcPoint = EndPoint;
				}
			}
		}
	}

	return this->AddLineSeg(AcGeLineSeg3d(SrcPoint, DstPoint), ColorIndex,
			IsHidden, LinkedObjectId, Tag, NodePoint, machiningId);
}
*/

/*
Acad::ErrorStatus RbCSDrawingPresenter::
SaveGraphObjSet(AcDbDwgFiler* Filer) const
{
	assert(Filer != 0);

	Acad::ErrorStatus es = Filer->writeItem((Adesk::UInt32)m_aGeObjects.GetSize());
	assert(es == Acad::eOk);

	RCAD::CGeDataCollection2::const_iterator Iter = m_aGeObjects.Begin();
	RCAD::CGeDataCollection2::const_iterator End = m_aGeObjects.End();
	for (; Iter != End; Iter++)
	{
		const RCAD::TStdGeObject& GeObject = *Iter;

		es = Filer->writeItem(static_cast<unsigned long>(GeObject.GetGeType()));
		assert(es == Acad::eOk);

		es = GeObject.DwgOut(Filer);
		assert(es == Acad::eOk);
	}

	return Filer->filerStatus();
}
*/

Acad::ErrorStatus RbCSDrawingPresenter::
LoadGraphObjSet(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);

	unsigned long Counter = 0;
	Acad::ErrorStatus es = Filer->readItem(&Counter);
	assert(es == Acad::eOk);

	RCAD::CGeDataCollection2::WriterPtr wr = m_aGeObjects.GetWriter();
	if ( wr->IsValid() == false )
		return Acad::eMakeMeProxy;

	while (Counter-- > 0)
	{
		AcGe::EntityId TypeId;

		es = Filer->readItem(reinterpret_cast<unsigned long*>(&TypeId));
		assert(es == Acad::eOk);

		RbCS::CGeObject GeObject;
		es = GeObject.DwgIn( Filer );
			assert(es == Acad::eOk);

		assert( TypeId == AcGe::kLineSeg3d );
		wr->Append() = GeObject;
	}

	//dispInfo.pPresenter = 0; // Reset disp info

	return Filer->filerStatus();
}

/*
Acad::ErrorStatus RbCSDrawingPresenter::
SaveGraphObjSet2(AcDbDwgFiler* Filer) const
{
	assert(Filer != 0);

	Acad::ErrorStatus es = Filer->writeItem( (Adesk::UInt32) m_aGeObjects.GetSize());
	assert(es == Acad::eOk);

	RCAD::CGeDataCollection2::const_iterator Iter = m_aGeObjects.Begin();
	RCAD::CGeDataCollection2::const_iterator End = m_aGeObjects.End();
	for (; Iter != End; Iter++)
	{
		RCAD::TStdGeObject GeObject = *Iter;

		es = Filer->writeItem(static_cast<unsigned long>(GeObject.GetGeType()));
		assert(es == Acad::eOk);

		es = GeObject.DwgOut2(Filer);
		assert(es == Acad::eOk);
	}

	return Filer->filerStatus();
}

*/

Acad::ErrorStatus RbCSDrawingPresenter::
LoadGraphObjSet2(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);

	unsigned long Counter = 0;
	Acad::ErrorStatus es = Filer->readItem(&Counter);
	assert(es == Acad::eOk);

	RCAD::CGeDataCollection2::WriterPtr wr = m_aGeObjects.GetWriter();
	if ( wr->IsValid() == false )
		return Acad::eMakeMeProxy;

	while (Counter-- > 0)
	{
		AcGe::EntityId TypeId;

		es = Filer->readItem(reinterpret_cast<unsigned long*>(&TypeId));
		assert(es == Acad::eOk);

		RbCS::CGeObject GeObject;
		//es = GeObject.DwgIn2( Filer );
		es = GeObject.DeprecatedDwgIn( Filer );
		assert(es == Acad::eOk);

		assert( TypeId == AcGe::kLineSeg3d );
		wr->Append() = GeObject;
	}

	//dispInfo.pPresenter = 0; // Reset disp info

	return Filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
SaveShortenings(AcDbDwgFiler* Filer) const
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	Acad::ErrorStatus es;

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; Iter++)
	{
		//es = Filer->writeItem(*Iter);
		es = Filer->writeHardPointerId(*Iter);
		assert(es == Acad::eOk);
	}

	es = Filer->writeHardPointerId(AcDbHardPointerId::kNull);
	assert(es == Acad::eOk);

	return Filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
LoadShortenings(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	m_Shortenings.clear();

	AcDbHardPointerId Id = AcDbHardPointerId::kNull;

	Acad::ErrorStatus es = Filer->readHardPointerId(&Id);

	while (Id.isNull() == false)
	{
		m_Shortenings.push_back(Id);

		TDwgShorteningPtr Shortening(Id, AcDb::kForRead, true);
	
		//Shortening->PresenterId = this->ObjectId;
		//Shortening->m_InPresenterLocator = --m_Shortenings.end();

		es = Filer->readHardPointerId(&Id);
		assert(es == Acad::eOk);
	}

	return Filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
writeShortenings(AcDbDwgFiler* filer) const
{
	assert(filer != 0);
	assert(filer->filerStatus() == Acad::eOk);

	Acad::ErrorStatus es = filer->writeUInt32(m_Shortenings.size());
	assert(es == Acad::eOk);

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; Iter++)
	{
		es = filer->writeHardPointerId(*Iter);
		assert(es == Acad::eOk);
	}

	return filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
readShortenings(AcDbDwgFiler* filer)
{
	assert(filer != 0);
	assert(filer->filerStatus() == Acad::eOk);

	RCAD::TSoftPtrIdVector().swap(m_Shortenings);

	AcDbHardPointerId id;

	Adesk::UInt32 size = 0;
	Acad::ErrorStatus es = filer->readUInt32(&size);

	for (Adesk::UInt32 i = 0; i < size; ++i)
	{
		es = filer->readHardPointerId(&id);

		if ( id.isNull() )
			continue;

		m_Shortenings.push_back(id);

		TDwgShorteningPtr shortening(id, AcDb::kForRead, true);
	
		//shortening->m_InPresenterLocator = --m_Shortenings.end();
	}

	return filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
SaveWeldSeams(AcDbDwgFiler* Filer) const
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	assert(this->isReadEnabled() == Adesk::kTrue);

	unsigned long Size = m_WeldSeams.size();

	Acad::ErrorStatus es = Filer->writeItem(Size);
	assert(es == Acad::eOk);

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_WeldSeams.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_WeldSeams.end();

	for (; Iter != End; ++Iter)
	{
		es = Filer->writeItem(*Iter);
		assert(es == Acad::eOk);
	}

	return Filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
LoadWeldSeams(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	assert(this->isWriteEnabled() == Adesk::kTrue);

	m_WeldSeams.clear();

	unsigned long Size = 0;

	Acad::ErrorStatus es = Filer->readItem(&Size);
	assert(es == Acad::eOk);

	m_WeldSeams.reserve(Size);

	RCAD::TSoftPtrIdVector::value_type Id;

	for (unsigned int i = 0; i < Size; ++i)
	{
		es = Filer->readItem(&Id);
		assert(es == Acad::eOk);

		m_WeldSeams.push_back(Id);
	}

	assert(m_WeldSeams.size() == Size);

	return Filer->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
writeUserObjEntries(AcDbDwgFiler* pFiler) const
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	assert(this->isReadEnabled() == Adesk::kTrue);

	Acad::ErrorStatus es = pFiler->writeUInt32(myUserObjEntries.size());
	assert(es == Acad::eOk);

	UserObjEntries::const_iterator iter = myUserObjEntries.begin();
	UserObjEntries::const_iterator iterEnd = myUserObjEntries.end();
	for (; iter != iterEnd; ++iter)
	{
		es = pFiler->writeSoftPointerId(iter->myObjectId);
		assert(es == Acad::eOk);

		es = AcGeDwgIO::outFields(pFiler, iter->myPlane);
		assert(es == Acad::eOk);
	}

	return pFiler->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
readUserObjEntries(AcDbDwgFiler* pFiler)
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	assert(this->isWriteEnabled() == Adesk::kTrue);

	Adesk::UInt32 size = 0;

	Acad::ErrorStatus es = pFiler->readUInt32(&size);
	assert(es == Acad::eOk);

	UserObjEntries entries;
	entries.reserve(size);

	for (Adesk::UInt32 i = 0; i < size; ++i)
	{
		UserObjEntry entry;

		AcDbSoftPointerId id;
		es = pFiler->readSoftPointerId(&id);
		assert(es == Acad::eOk);

		entry.myObjectId = id;

		es = AcGeDwgIO::inFields(pFiler, entry.myPlane);
		assert(es == Acad::eOk);

		entries.push_back(entry);
	}

	myUserObjEntries.swap(entries);

	return pFiler->filerStatus();
}

void RbCSDrawingPresenter::EraseAll()
{
	myExtents = AcDbExtents();
	m_aGeObjects.Clear();
	//dispInfo.pPresenter = 0; // Reset disp info
	m_circs.clear();
	m_aDrillSegCash.Clear();
}

void RbCSDrawingPresenter::
AddShortening(const AcDbObjectId& Id)
{
	assert(Id.isValid() == true);

	TDwgShorteningPtr Shortening(Id, AcDb::kForWrite, true);

	// fix bug 3012
	if ( Shortening.openStatus() != Acad::eOk ) {
		return;
	}
	// end 3012

	this->AddShortening(Shortening.object());

	Acad::ErrorStatus es = Shortening->close();
	assert(es == Acad::eOk);

	this->updateHatch();
}

void RbCSDrawingPresenter::
RemoveShortening(const AcDbObjectId& Id)
{
	assert(Id.isValid() == true);

	TDwgShorteningPtr Shortening(Id, AcDb::kForWrite);

	this->RemoveShortening(Shortening.object());

	if (Shortening->isErased() == Adesk::kFalse)
	{
		Shortening->erase();
	}
}

void RbCSDrawingPresenter::
AddShortening(RbCSDwgShortening* Shortening)
{
	assert(Shortening != 0);

	this->assertWriteEnabled();

	//dispInfo.pPresenter = 0; // Reset disp info

	m_Shortenings.push_back(Shortening->ObjectId);

	Shortening->SetPresenterId(this->objectId());

	// fix bug 3129
	myExtents = AcDbExtents();
	// end 3129

	gPresenterStateObserver.SetStateChanged();

	m_aGeObjects.UpdateToken();
}

void RbCSDrawingPresenter::
RemoveShortening(RbCSDwgShortening* Shortening)
{
	assert(Shortening != 0);
	assert(Shortening->PresenterId == this->ObjectId);
	assert(Shortening->isWriteEnabled() == Adesk::kTrue);

	this->assertWriteEnabled();

	//dispInfo.pPresenter = 0; // Reset disp info

	RCAD::TSoftPtrIdVector::iterator iter = 
			std::find(m_Shortenings.begin(), m_Shortenings.end(),
			Shortening->objectId());

	if (iter != m_Shortenings.end())
	{
		m_Shortenings.erase(iter);
	}

	Acad::ErrorStatus es = Shortening->downgradeOpen();
	assert(es == Acad::eOk);

	this->updateHatch();

	es = Shortening->upgradeOpen();
	assert(es == Acad::eOk);

	// fix bug 3129
	myExtents = AcDbExtents();
	// end 3129

	gPresenterStateObserver.SetStateChanged();
	
	m_aGeObjects.UpdateToken();
}

void RbCSDrawingPresenter::RemoveAllShortenings()
{
	this->assertWriteEnabled();

	RCAD::TSoftPtrIdVector::iterator iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::iterator end = m_Shortenings.end();
	for (; iter != end; iter++)
	{
		TDwgShorteningPtr Shortening(*iter, AcDb::kForWrite, true);

		Shortening->SetPresenterId(AcDbObjectId::kNull);

		Acad::ErrorStatus es = Shortening->erase();
		assert(es == Acad::eOk);
	}

	RCAD::TSoftPtrIdVector().swap(m_Shortenings);

	//dispInfo.pPresenter = 0; // Reset disp info

	this->updateHatch();
	// fix bug 3129
	myExtents = AcDbExtents();
	// end 3129

	gPresenterStateObserver.SetStateChanged();

	m_aGeObjects.UpdateToken();
}

void RbCSDrawingPresenter::
UnbindShortenings()
{
	this->assertWriteEnabled();

	RCAD::TSoftPtrIdVector().swap(m_Shortenings);

	//dispInfo.pPresenter = 0; // Reset disp info
	m_aGeObjects.UpdateToken();
}

void RbCSDrawingPresenter::
UnbindHatch()
{
	this->assertWriteEnabled();

	myHatch.clear();

	myHatchId.setNull();
}

void RbCSDrawingPresenter::
RestoreAllShortenings(AcDbDwgFiler* Filer)
{
	assert(Filer != 0);
	assert(Filer->filerStatus() == Acad::eOk);

	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		assert(UndoFiler->filerStatus() == Acad::eOk);

		Acad::ErrorStatus es = UndoFiler->writeItem(
				reinterpret_cast<long>(desc()));
		assert(es == Acad::eOk);

		es = UndoFiler->writeItem(static_cast<long>(uocRestoreAllShortenings));
		assert(es == Acad::eOk);
	}

	//dispInfo.pPresenter = 0; // Reset disp info

	Acad::ErrorStatus es = this->LoadShortenings(Filer);
	assert(es == Acad::eOk);
}

const RCAD::CGeDataCollection2& RbCSDrawingPresenter::
GetOrigGeSet() const
{
	return m_aGeObjects;
}

/////////////////////////////////////////////////////////////////////
template < class TGeColln, class TClipBound >
void ApplyShortenings( const RbCSDrawingPresenter& prs, const RCAD::TSoftPtrIdVector& aShorts, TGeColln& aGeObjects )
{
	double ShortGap = RbCSDwgShortening::GetShortGap( prs.ViewportId ) / 2.0;
	assert(ShortGap >= 0.0);

	RCAD::TSoftPtrIdVector::const_iterator shortenings_iter = aShorts.begin();
	RCAD::TSoftPtrIdVector::const_iterator shortenings_end = aShorts.end();

	for (; shortenings_iter != shortenings_end; ++shortenings_iter)
	{
		AcDbObjectId shortening_id = *shortenings_iter;
		assert(shortening_id.isValid() == true);

		TDwgShorteningPtr shortening(shortening_id, AcDb::kForRead);

		if (shortening.openStatus() == Acad::eOk)
		{
			TGeColln clipped_set;

			unsigned int blocks_count = shortening->BlocksCount;
			for (int i = 0; i < blocks_count; i++)
			{
				const RbCSDwgShortening::TBlock& block =
						shortening->GetBlock(i);

				AcGeBoundBlock2d bounds = block.Bounds;

				AcGePoint2d min_point, max_point;
				bounds.getMinMaxPoints(min_point, max_point);

				TClipBound clip_boundary( min_point, max_point );

				//AcGeClipBoundary2d clip_boundary =
				//		AcGeClipBoundary2d(min_point, max_point);

				aGeObjects.Cut( clip_boundary, &clipped_set,
						block.Translation -
						block.Translation.normal() * ShortGap);
			}

			/* For drillings analisys whole clipped set must be empty
			if (clipped_set.IsEmpty() == false)
			{
				aGeObjects = clipped_set;
			}
			*/
			aGeObjects = clipped_set;
		}
	}
}

template < class TGeColln, class TClipBound  >
void ApplyClipping( const RbCSDrawingPresenter& prs, TGeColln& aGeObjects )
{
	TGeColln clipped_set;

	AcGePoint2d p1, p2;
	//AcGeClipBoundary2d clipBound;
	TClipBound clipBound;

	if ( prs.IsClipped() )
	{
		prs.GetClipping( p1, p2 );
		clipBound.set( p1, p2 );

		aGeObjects.Cut( clipBound, &clipped_set );
	}

	/* For drillings analisys whole clipped set must be empty
	if (clipped_set.IsEmpty() == false)
	{
		aGeObjects = clipped_set;
	}
	*/
	aGeObjects = clipped_set;
}
/////////////////////////////////////////////////////////////////////

RCAD::CGeDataCollection& RbCSDrawingPresenter::
GetDrillDispGeSet( RCAD::CGeDataCollection& origSet, bool useShortennings, bool useClipping ) const
{
	if ( useShortennings )
		ApplyShortenings<RCAD::CGeDataCollection,AcGeClipBoundary2d>( *this, m_Shortenings, origSet );

	if ( useClipping && m_IsClipped )
		ApplyClipping<RCAD::CGeDataCollection,AcGeClipBoundary2d>( *this, origSet );

	return origSet;
}

RCAD::CGeDataCollection2 RbCSDrawingPresenter::
GetDispGeSet( bool useShortennings, bool useClipping ) const
{
	if ( Is3dView() || !( useShortennings || useClipping ) )
		return GetOrigGeSet();

	RCAD::CGeDataCollection2 primitive_set_2d;

	primitive_set_2d = m_aGeObjects;

	if ( useShortennings )
		ApplyShortenings<RCAD::CGeDataCollection2,AcGeClipBoundary2d>( *this, m_Shortenings, primitive_set_2d );

	if ( useClipping && m_IsClipped )
		ApplyClipping<RCAD::CGeDataCollection2,AcGeClipBoundary2d>( *this, primitive_set_2d );

	return primitive_set_2d;
}

const AcGeVector3d& RbCSDrawingPresenter::
GetViewDir() const
{
	assertReadEnabled();

	return m_ViewDir;
}

void RbCSDrawingPresenter::
SetViewDir(const AcGeVector3d& Value)
{
	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		UndoFiler->writeItem((long)desc());
		UndoFiler->writeItem((long)uocSetViewDir);

		AcGeDwgIO::outFields(UndoFiler, m_ViewDir);
	}

	m_ViewDir = Value;
}

const AcGeVector3d& RbCSDrawingPresenter::
GetUpVector() const
{
	assertReadEnabled();

	return m_UpVector;
}

void RbCSDrawingPresenter::
SetUpVector(const AcGeVector3d& Value)
{
	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		UndoFiler->writeItem((long)desc());
		UndoFiler->writeItem((long)uocSetUpVector);

		AcGeDwgIO::outFields(UndoFiler, m_UpVector);
	}

	m_UpVector = Value;
}

bool RbCSDrawingPresenter::
GetHiddenLinesIsVisible() const
{
	assertReadEnabled();

	return m_HiddenLinesIsVisible;
}

void RbCSDrawingPresenter::
SetHiddenLinesIsVisible(bool Value)
{
	assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = undoFiler();
	if (UndoFiler != 0)
	{
		Acad::ErrorStatus es = UndoFiler->writeItem((long)desc());
		assert(es == Acad::eOk);

		es = UndoFiler->writeItem((long)uocSetHiddenLinesIsVisible);
		assert(es == Acad::eOk);

		es = UndoFiler->writeItem(m_HiddenLinesIsVisible);
		assert(es == Acad::eOk);
	}

	m_HiddenLinesIsVisible = Value;
}

const AcGeMatrix3d& RbCSDrawingPresenter::
GetOCS() const
{
	assertReadEnabled();

	return m_OCS;
}

const AcDbObjectId& RbCSDrawingPresenter::
GetViewportId() const
{
	assertReadEnabled();

	return m_ViewportId;
}

void RbCSDrawingPresenter::
SetViewportId(const AcDbObjectId& value)
{
	if (m_ViewportId != value)
	{
		this->assertWriteEnabled();

		m_ViewportId = value;
	}
}

const AcDbObjectId& RbCSDrawingPresenter::
GetLinkedObjectId() const
{
	assertReadEnabled();

	return m_ObjectId;
}

void RbCSDrawingPresenter::
GetShortenings(vector<AcDbObjectId>& Vector) const
{
	assertReadEnabled();

	Vector.clear();
	Vector.reserve(m_Shortenings.size());

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; Iter++)
	{
		Vector.push_back(*Iter);
	}
}

void RbCSDrawingPresenter::
ToShortedLocation(AcGePoint2dArray& Points, AcArray<bool>& HiddenFlags) const
{
	assert(Points.isEmpty() == false);

	this->assertReadEnabled();

	bool InitValue = !m_Shortenings.empty();

	int PointsLength = Points.length();
	HiddenFlags.setLogicalLength(PointsLength);
	/*for (int i = 0; i < PointsLength; i++)
	{
		HiddenFlags[i] = InitValue;
	}*/int i = 0;

	AcArray<int> Counters(PointsLength);
	Counters.setLogicalLength(PointsLength);
	for (i = 0; i < PointsLength; i++)
	{
		Counters[i] = 0;
	}

	double ShortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();

	for (; Iter != End; Iter++)
	{
		AcDbObjectId ShorteningId = *Iter;
		assert(ShorteningId.isValid() == true);

		TDwgShorteningPtr Shortening(ShorteningId, AcDb::kForRead);

		if (Shortening.openStatus() != Acad::eOk)
			continue;

		RbCSDwgShortening::TBlockVector::const_iterator BlocksIter =
				Shortening->m_Blocks.begin();
		RbCSDwgShortening::TBlockVector::const_iterator BlocksEnd =
				Shortening->m_Blocks.end();
		for (; BlocksIter != BlocksEnd; BlocksIter++)
		{
			const RbCSDwgShortening::TBlock& Block = *BlocksIter;

			AcGeVector2d Translation = Block.Translation -
					Block.Translation.normal() * ShortGap;

			AcGePoint2d MinPoint, MaxPoint;
			Block.Bounds.getMinMaxPoints(MinPoint, MaxPoint);
			//MinPoint.transformBy(Translation);
			//MaxPoint.transformBy(Translation);

			int Length = Points.length();
			for (int i = 0; i < Length; i++)
			{
				AcGePoint2d& Point = Points[i];

				if ((Point.x >= MinPoint.x) && (Point.x <= MaxPoint.x) &&
						(Point.y >= MinPoint.y) && (Point.y <= MaxPoint.y))
				{
					Point.transformBy(Translation);

					++Counters[i];
				}
			}
		}
	}

	int ShortenningCount = m_Shortenings.size();

	for (i = 0; i < PointsLength; i++)
	{
		int Value = Counters[i];

		if (Counters[i] == ShortenningCount)
		{
			HiddenFlags[i] = false;
		}
		else
		{
			HiddenFlags[i] = true;
		}
	}

	if (m_IsClipped == true)
	{
		AcGeClipBoundary2d ClipBound;
		m_ClipBound.Get(ClipBound);

		AcGe::ClipCondition ClipCondition;
		AcGe::ClipError ClipError;
		AcGePoint2dArray Pnts(2), ClippedPnts;

		for (i = 0; i < PointsLength; i++)
		{
			Pnts.append(Points[i]);
			Pnts.append(Points[i]);

			ClipError = ClipBound.clipPolyline(Pnts, ClippedPnts,
					ClipCondition);

			if (ClipCondition != AcGe::kAllSegmentsInside)
			{
				HiddenFlags[i] = true;
			}
		}
	}
}

void RbCSDrawingPresenter::
ToOriginalLocation(AcGePoint2dArray& Points, bool InvertDirection) const
{
	assert(Points.isEmpty() == false);

	assertReadEnabled();

	double ShortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	RCAD::TSoftPtrIdVector::const_reverse_iterator Iter = m_Shortenings.rbegin();
	RCAD::TSoftPtrIdVector::const_reverse_iterator End = m_Shortenings.rend();

	for (; Iter != End; Iter++)
	{
		AcDbObjectId ShorteningId = *Iter;
		assert(ShorteningId.isValid() == true);

		TDwgShorteningPtr Shortening(ShorteningId, AcDb::kForRead);

		if (Shortening.openStatus() != Acad::eOk)
			continue;

		RbCSDwgShortening::TBlockVector::const_iterator BlocksIter =
				Shortening->m_Blocks.begin();
		RbCSDwgShortening::TBlockVector::const_iterator BlocksEnd =
				Shortening->m_Blocks.end();
		for (; BlocksIter != BlocksEnd; BlocksIter++)
		{
			const RbCSDwgShortening::TBlock& Block = *BlocksIter;

			AcGeVector2d Translation = Block.Translation -
					Block.Translation.normal() * ShortGap;

			if (InvertDirection == true)
			{
				Translation.negate();
			}

			AcGePoint2d MinPoint, MaxPoint;
			Block.Bounds.getMinMaxPoints(MinPoint, MaxPoint);

			MinPoint.transformBy(Translation);
			MaxPoint.transformBy(Translation);

			int Length = Points.length();
			for (int i = 0; i < Length; i++)
			{
				AcGePoint2d& Point = Points[i];

				if (((Point.x >= MinPoint.x) && (Point.x <= MaxPoint.x)) &&
						((Point.y >= MinPoint.y) && (Point.y <= MaxPoint.y)))
				{
					Point.transformBy(-Translation);
				}
			}
		}
	}
}

#include "..\RbCSHeader\RbCSService.h"
#include "..\RbCSHeader\IDbxService.h"

void RbCSDrawingPresenter::
ToShortedPresentation(const AcGeLineSeg2d& LineSeg,
		AcArray<AcGeLineSeg2d, AcArrayObjectCopyReallocator<AcGeLineSeg2d> >&
		Segments) const
{
	assert(Segments.isEmpty() == true);

	Segments.append(LineSeg);

	this->assertReadEnabled();

	double ShortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	bool outside = true;

	RCAD::TSoftPtrIdVector::const_iterator Iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator End = m_Shortenings.end();
	for (; Iter != End; ++Iter)
	{
		AcArray<AcGeLineSeg2d, AcArrayObjectCopyReallocator<AcGeLineSeg2d> >
				NewSegments;

		AcDbObjectId ShorteningId = *Iter;
		assert(ShorteningId.isValid() == true);

		TDwgShorteningPtr Shortening(ShorteningId, AcDb::kForRead);

		RbCSDwgShortening::TBlockVector::const_iterator BlocksIter =
					Shortening->m_Blocks.begin();
		RbCSDwgShortening::TBlockVector::const_iterator BlocksEnd =
					Shortening->m_Blocks.end();
		for (; BlocksIter != BlocksEnd; BlocksIter++)
		{
			const RbCSDwgShortening::TBlock& Block = *BlocksIter;

			AcGeVector2d Translation = Block.Translation -
					Block.Translation.normal() * ShortGap;

			AcGePoint2d Min, Max;
			Block.Bounds.getMinMaxPoints(Min, Max);

			AcGeClipBoundary2d ClipBoundary(Min, Max);

			size_t segmentsNumber = Segments.length();

			for (int i = 0; i < segmentsNumber; ++i)
			{
				const AcGeLineSeg2d& LineSeg = Segments[i];

				AcGe::ClipCondition ClipCondition;
				AcGe::ClipError ClipError;
				AcGePoint2dArray Points, ClippedPoints;

				Points.append(LineSeg.startPoint());
				Points.append(LineSeg.endPoint());

				ClipError = ClipBoundary.clipPolyline(Points, ClippedPoints,
						ClipCondition);

				if ((ClipError == AcGe::kOk) &&
						((ClipCondition == AcGe::kSegmentsIntersect) ||
						(ClipCondition == AcGe::kAllSegmentsInside)))
				{
					if (ClippedPoints.length() == 1)
					{
						NewSegments.append(AcGeLineSeg2d(
							ClippedPoints[0] + Translation,
							ClippedPoints[0] + Translation));
					}
					else
					{
						NewSegments.append(AcGeLineSeg2d(
								ClippedPoints[0] + Translation,
								ClippedPoints[1] + Translation));
					}

					outside = false;
				}
			}
		}

		/*if (NewSegments.isEmpty() == false)
			Segments = NewSegments;*/

		// If NewSegments is empty the output must be empty too
		Segments = NewSegments;

	}

	if (outside == true)
	{
		Segments.setPhysicalLength(0);
	}

	if (m_Shortenings.empty() == true)
	{
		Segments.append(LineSeg);
	}

	if (m_IsClipped == true)
	{
		AcGePoint2d cornerA, cornerB;
		AcGeClipBoundary2d ClipBoundary;
		m_ClipBound.Get(ClipBoundary);
		m_ClipBound.Get(cornerA, cornerB);
		const AcGeBoundBlock2d clipRect(cornerA , cornerB);

		AcArray<AcGeLineSeg2d, AcArrayObjectCopyReallocator<AcGeLineSeg2d> >
				NewSegments;

		for (int i = 0; i < Segments.length(); i++)
		{
			const AcGeLineSeg2d& LineSeg = Segments[i];
			const AcGePoint2d ptStart = LineSeg.startPoint(); 
			const AcGePoint2d ptEnd = LineSeg.endPoint();

			if (ptStart.isEqualTo(ptEnd, algo::kTol))
			{
				if (clipRect.contains(ptStart))
					NewSegments.append(AcGeLineSeg2d(ptStart, ptEnd));
			}
			else
			{
				AcGe::ClipCondition ClipCondition;
				AcGe::ClipError ClipError;
				AcGePoint2dArray Points, ClippedPoints;

				Points.append(ptStart);
				Points.append(ptEnd);

				ClipError = ClipBoundary.clipPolyline(Points, ClippedPoints,
					ClipCondition);

				if ((ClipError == AcGe::kOk) &&
					(ClippedPoints.isEmpty() == false) &&
					(ClippedPoints.length() > 1))
				{
					if ((ClipCondition == AcGe::kSegmentsIntersect) ||
						(ClipCondition == AcGe::kAllSegmentsInside))
					{
						NewSegments.append(AcGeLineSeg2d(ClippedPoints[0],
							ClippedPoints[1]));
					}
				}
			}
		}

		Segments = NewSegments;
	}
}

void RbCSDrawingPresenter::
FindShortedLocations(AcGePoint2dArray& points, AcArray<bool>& hiddenFlags,
		const AcDbObjectId& firstShorteningId) const
{
	assert(points.isEmpty() == false);

	int pointsLength = points.length();

	this->assertReadEnabled();

	double gap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	hiddenFlags.setLogicalLength(pointsLength);
	hiddenFlags.setAll(true);

	RCAD::TSoftPtrIdVector::const_iterator shIter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator shIterEnd = m_Shortenings.end();

	if (firstShorteningId.isNull() == false)
		shIter = std::find(shIter, shIterEnd, firstShorteningId);

	if (++shIter == shIterEnd)
	{
		hiddenFlags.setAll(false);
	}

	for (shIter; shIter != shIterEnd; ++shIter)
	{
		TDwgShorteningPtr shortening(*shIter, AcDb::kForRead);

		RbCSDwgShortening::TBlockVector blocks = shortening->m_Blocks;

		RbCSDwgShortening::TBlockVector::const_iterator blocksIter =
				blocks.begin();
		RbCSDwgShortening::TBlockVector::const_iterator blocksIterEnd =
				blocks.end();
		for (; blocksIter != blocksIterEnd; ++blocksIter)
		{
			const RbCSDwgShortening::TBlock& block = *blocksIter;

			AcGeBoundBlock2d blockBound = block.Bounds;

			AcGeVector2d blockTranslation = block.Translation;

			for (int i = 0; i < pointsLength; ++i)
			{
				AcGePoint2d& point = points[i];

				if (blockBound.contains(point) == Adesk::kTrue)
				{
					point += blockTranslation - blockTranslation.normal() * gap;

					hiddenFlags[i] = false;
				}
			}
		}
	}
}

AcDbObjectId RbCSDrawingPresenter::
GetIdByGSMarker(unsigned long GSMarker) const
{
	this->assertReadEnabled();

	AcDbObjectId ObjectId = AcDbObjectId::kNull;

	if ( GSMarker < m_minGSArcMarker )
	{
		RCAD::CGeDataCollection2 DisplayedSet = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );
		RCAD::CGeDataCollection2::ReaderPtr rd = DisplayedSet.GetReader();

		if ( GSMarker < rd->GetSize() )
		{
			const RbCS::CGeObject2d& geObject = (*rd)[GSMarker];
			ObjectId = geObject.GetLinkedObjId();
		}
	}
	else
	{
		RCAD::CGeArcCollection ArcSet = GetOrigGeArcSet();

		int pos = GSMarker - m_minGSArcMarker;
		if ( pos < ArcSet.size() )
		{
			const RbCS::CGeArc& geArc = ArcSet[ pos ];
			ObjectId = geArc.GetLinkedObjId();
		}
	}

	return ObjectId;
}

AcGeLineSeg3d RbCSDrawingPresenter::
GetSegmentByGSMarker(unsigned long GSMarker) const
{
	this->assertReadEnabled();

	AcGeLineSeg3d segment;

	RCAD::CGeDataCollection2 DisplayedSet = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );
	RCAD::CGeDataCollection2::ReaderPtr rd = DisplayedSet.GetReader();

	if ( GSMarker < rd->GetSize() )
	{
		const RbCS::CGeObject2d& geObject = (*rd)[GSMarker];
		RbCS::TEntity2dPtr e2d = geObject.AsEntity2d();
		const AcGeLineSeg2d* pLineSeg = static_cast< const AcGeLineSeg2d* >( e2d.get() );
		assert( pLineSeg );

		if (pLineSeg != 0)
		{
			segment.set(AcGePoint3d(AcGePlane::kXYPlane, pLineSeg->startPoint()),
					AcGePoint3d(AcGePlane::kXYPlane, pLineSeg->endPoint()));
			segment.transformBy(this->GetOCS());
		}

	}
	return segment;
}

void RbCSDrawingPresenter::
AssignShortennings(const AcGeLine2d& Axes,
		const std::vector<double>& Locations,
		std::vector<AcDbObjectId>& ShorteningIds)
{
	this->assertWriteEnabled();

	// fix bug TT#3222
	RemoveAllShortenings();
	// end 3222

	AcDbExtents Extents;

	Acad::ErrorStatus es = this->getGeomExtents(Extents);
	assert(es == Acad::eOk);

	Extents.transformBy(this->OCS.inverse());

	double ShortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	AcGePoint2d FirstPoint = Extents.minPoint().convert2d(AcGePlane::kXYPlane);
	AcGePoint2d SecondPoint = Extents.maxPoint().convert2d(
			AcGePlane::kXYPlane);

	AcGePoint2d MinPoint = min_point(FirstPoint, SecondPoint);
	AcGePoint2d MaxPoint = max_point(FirstPoint, SecondPoint);

	rect_utils::t_rect ExtentsRect(MinPoint.x, MinPoint.y,
			MaxPoint.x, MaxPoint.y);

	AcGePoint2dArray Points(5);
	Points.append(MinPoint);
	Points.append(AcGePoint2d(MinPoint.x, MaxPoint.y));
	Points.append(MaxPoint);
	Points.append(AcGePoint2d(MaxPoint.x, MinPoint.y));
	Points.append(MinPoint);
	AcGePolyline2d ExtentsPolyline(Points);

	AcGeTol Tol;
	Tol.setEqualPoint(0.001);
	Tol.setEqualVector(0.001);

	double Offset = 0.0;

	std::vector<double>::const_iterator Iter = Locations.begin();
	std::vector<double>::const_iterator End = Locations.end();
	while (Iter != End)
	{
		double FirstParam = *(Iter++);
		double SecondParam = *(Iter++);

		AcGePoint2d FirstPoint = Axes.evalPoint(FirstParam) -
				Axes.direction().normal() * Offset;
		AcGePoint2d SecondPoint = Axes.evalPoint(SecondParam) -
				Axes.direction().normal() * Offset;

		AcGeLine2d FirstLine, SecondLine;

		Axes.getPerpLine(FirstPoint, FirstLine);
		Axes.getPerpLine(SecondPoint, SecondLine);

		AcGeCurveCurveInt2d FirstInt(ExtentsPolyline, FirstLine, Tol);
		assert(FirstInt.numIntPoints() == 2);
		if (FirstInt.numIntPoints() < 2)
			continue;

		AcGeCurveCurveInt2d SecondInt(ExtentsPolyline, SecondLine, Tol);
		assert(SecondInt.numIntPoints() == 2);
		if (SecondInt.numIntPoints() < 2)
			continue;

		AcGePoint2d MinCorner = min_point(
				min_point(FirstInt.intPoint(0), FirstInt.intPoint(1)),
				min_point(SecondInt.intPoint(0), SecondInt.intPoint(1)));

		AcGePoint2d MaxCorner = max_point(
				max_point(FirstInt.intPoint(0), FirstInt.intPoint(1)),
				max_point(SecondInt.intPoint(0), SecondInt.intPoint(1)));

		rect_utils::t_rect ClipRect(MinCorner.x, MinCorner.y,
				MaxCorner.x, MaxCorner.y);

		rect_utils::t_rect r1 = ExtentsRect, r2 = ClipRect, r3, r4;

		double tol = AcGeContext::gTol.equalPoint();

		int Result = rect_utils::rect_fragmentation(r1, r2, r3, r4, tol);
		if (Result == 2)
		{
			RbCSDwgShortening* Shortening = new RbCSDwgShortening;
			assert(Shortening != 0);

			Acad::ErrorStatus es = Shortening->setLayer(this->layerId());

			bool Result = Shortening->Create(this->Database);
			assert(Result == true);

			AcDbObjectId ShorteningId = Shortening->ObjectId;
			assert(ShorteningId.isValid() == true);

			RbCSDwgShortening::TBlock Block;

			AcGeVector2d Translation;

			bool IsHideout = true;
			bool IsHorizontal = true;

			static const double swell_value = 1e+7;

			if ((fabs(r1.height() - r2.height()) <= tol) &&
					(fabs(r1.bottom() - r2.bottom()) <= tol))
			{
				Translation.set(ClipRect.width() / 2.0, 0.0);

				IsHideout = false;
				IsHorizontal = false;

				r1.swell(swell_value, 0.0, swell_value, swell_value);

				r2.swell(0.0, swell_value, swell_value, swell_value);
			}
			else if ((fabs(r1.width() - r2.width()) <= tol) &&
					(fabs(r1.left() - r2.left()) <= tol))
			{
				Translation.set(0.0, ClipRect.height() / 2.0);

				IsHideout = false;
				IsHorizontal = true;

				r1.swell(swell_value, swell_value, swell_value, 0.0);

				r2.swell(swell_value, swell_value, 0.0, swell_value);
			}

			Offset += Translation.length() - ShortGap;

			Block.Bounds = AcGeBoundBlock2d(
					AcGePoint2d(r2.left(), r2.bottom()),
					AcGePoint2d(r2.right(), r2.top()));
			Block.Translation = -Translation;

			if (IsHideout == false)
			{
				if (IsHorizontal == true)
				{
					Block.BreaklinePosition = RbCSDwgShortening::TBlock::blpBottom;
				}
				else
				{
					Block.BreaklinePosition = RbCSDwgShortening::TBlock::blpLeft;
				}
			}
			else
			{
			}

			Shortening->AppendBlock(Block);

			Block.Bounds = AcGeBoundBlock2d(
					AcGePoint2d(r1.left(), r1.bottom()),
					AcGePoint2d(r1.right(), r1.top()));
			Block.Translation = Translation;

			if (IsHideout == false)
			{
				if (IsHorizontal == true)
				{
					Block.BreaklinePosition = RbCSDwgShortening::TBlock::blpTop;
				}
				else
				{
					Block.BreaklinePosition = RbCSDwgShortening::TBlock::blpRight;
				}
			}
			else
			{
			}

			Shortening->AppendBlock(Block);

			es = Shortening->close();
			assert(es == Acad::eOk);

			ShorteningIds.push_back(ShorteningId);

			this->AddShortening(ShorteningId);
		}
	}
	m_aGeObjects.UpdateToken();
}

void RbCSDrawingPresenter::
GetCenterSnaps(AcGePoint3dArray& Snaps) const
{
	using namespace RbCS;
	RCAD::CGeDataCollection2 displayed_set = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );
	RCAD::CGeDataCollection2::ReaderPtr rd = displayed_set.GetReader();

	AcGePoint2dArray Snaps2d;

	AcGeMatrix3d Transf = m_OCS;

	unsigned int Count = rd->GetSize();
	for (unsigned int i = 0; i < Count; i++)
	{
		const CGeObject2d& geObject = (*rd)[i];

		if ( geObject.GetTag() == 2 )
		{
			// TODO: after nearest changes we can get snaps from
			// approximated circles

			//AcGePoint3d Point = geObject.NodePoint;
			//Snaps2d.append(AcGePoint2d(Point.x, Point.y));
		}

/*		if ( geObject.GetGeType() == AcGe::kCircArc2d )
		{
			AcGePoint2d center;
			double radius;

			geObject.GetCircleParams( center, radius );

			Snaps2d.append( center );
		} */
	}

	if (Snaps2d.isEmpty() == false)
	{
		AcArray<bool> HiddenFlags;
		this->ToShortedLocation(Snaps2d, HiddenFlags);

		Count = Snaps2d.length();
		for (int i = 0; i < Count; i++)
		{
			if (HiddenFlags[i] == false)
			{
				AcGePoint2d& Point2d = Snaps2d[i];

				AcGePoint3d Point3d(Point2d.x, Point2d.y, 0.0);
				Point3d.transformBy(Transf);

				Snaps.append(Point3d);
			}
		}
	}
}

bool RbCSDrawingPresenter::
Is3dView() const
{
	bool b3D = false;
	AcDbObjectPointer<RbCSViewport> Viewport(this->ViewportId, AcDb::kForRead);
	if( Viewport.openStatus() == Acad::eOk )
		b3D = Viewport->isKindOf(RbCSIsometricViewport::desc()) ||
		Viewport->isKindOf(RbCSDimetricViewport::desc());

	return( b3D );
}

bool RbCSDrawingPresenter::
IsClipped() const
{
	this->assertReadEnabled();

	return m_IsClipped;
}

void RbCSDrawingPresenter::
SetClipping(const AcGePoint2d& CornerA, const AcGePoint2d& CornerB)
{
	this->assertWriteEnabled();

	if (CornerA != CornerB)
	{
		m_ClipBound.Set(CornerA, CornerB);

		m_IsClipped = true;
	}

	//dispInfo.pPresenter = 0;
	myExtents = AcDbExtents();
}

void RbCSDrawingPresenter::
GetClipping( AcGePoint2d& CornerA, AcGePoint2d& CornerB ) const
{
	this->assertReadEnabled();

	if( m_IsClipped )
		m_ClipBound.Get( CornerA, CornerB );
}

AcDbObjectId RbCSDrawingPresenter::
GetSchemeId() const
{
	assert(this->isReadEnabled() == Adesk::kTrue);

	this->assertReadEnabled();

	return m_SchemeId;
}

void RbCSDrawingPresenter::
SetSchemeId(const AcDbObjectId& Value)
{
	assert(this->isWriteEnabled() == Adesk::kTrue);

	if (m_SchemeId != Value)
	{
		this->assertWriteEnabled(false);

		AcDbDwgFiler* UndoFiler = this->undoFiler();

		if (UndoFiler != 0)
		{
			UndoFiler->writeItem((long)desc());
			UndoFiler->writeItem((long)uocSetSchemeId);

			Acad::ErrorStatus es = UndoFiler->writeItem(m_SchemeId);
			assert(es == Acad::eOk);
		}

		m_SchemeId = Value;
	}
}

void RbCSDrawingPresenter::
GetWeldSeams(RCAD::TObjectIdVector& Ids)
{
	assert(this->isReadEnabled() == Adesk::kTrue);

	this->assertReadEnabled();

	Ids.assign(m_WeldSeams.begin(), m_WeldSeams.end());
}

void RbCSDrawingPresenter::
SetWeldSeams(const RCAD::TObjectIdVector& Ids)
{
	assert(this->isWriteEnabled() == Adesk::kTrue);

	this->assertWriteEnabled(false);

	AcDbDwgFiler* UndoFiler = this->undoFiler();

	if (UndoFiler != 0)
	{
		Acad::ErrorStatus es = UndoFiler->writeItem((long)desc());
		assert(es == Acad::eOk);

		es = UndoFiler->writeItem((long)uocSetWeldSeams);
		assert(es == Acad::eOk);

		es = this->SaveWeldSeams(UndoFiler);
		assert(es == Acad::eOk);
	}

	m_WeldSeams.clear();

	m_WeldSeams.reserve(Ids.size());

	RCAD::TObjectIdVector::const_iterator Iter = Ids.begin();
	RCAD::TObjectIdVector::const_iterator End = Ids.end();

	for (; Iter != End; ++Iter)
	{
		m_WeldSeams.push_back(*Iter);
	}
}

void RbCSDrawingPresenter::
addHatchLoop(const AcGePoint2dArray& points)
{
	assert(this->isWriteEnabled() == Adesk::kTrue);

	this->assertWriteEnabled();

	myHatch.addLoop(points);

	this->updateHatch();
}

void RbCSDrawingPresenter::
updateHatch()
{
	RbCSViewportPtr viewport(m_ViewportId, AcDb::kForRead);
	if (viewport.openStatus() == Acad::eOk)
	{
		this->updateHatch(viewport.object());
	}
}

void RbCSDrawingPresenter::
updateHatch(const RbCSViewport* viewport)
{
	if (myHatchId.isNull() == true)
	{
		AcDbDatabase* database = this->database();
		assert(database != 0);
		if (database == NULL)
			return;

		ARX::DbBlockTablePtr blockTable(database->blockTableId(),
				AcDb::kForRead);
		if (blockTable.openStatus() != Acad::eOk)
			return;

		AcDbObjectId modelSpaceId;
		Acad::ErrorStatus es = blockTable->getAt(ACDB_MODEL_SPACE, modelSpaceId);
		assert(es == Acad::eOk);

		AcDbBlockTableRecordPointer modelSpace(modelSpaceId, AcDb::kForWrite);
		if (modelSpace.openStatus() != Acad::eOk)
			return;

		AcDbObjectPointer<AcDbHatch> pHatch;

		es = pHatch.create();
		assert(es == Acad::eOk);

		es = modelSpace->appendAcDbEntity(pHatch.object());
		assert(es == Acad::eOk);

		myHatchId = pHatch->objectId();
	}

	AcDbObjectPointer<AcDbHatch> pHatch(myHatchId, AcDb::kForWrite, true);

	if (pHatch.openStatus() == Acad::eOk)
	{
		if (pHatch->isErased() == Adesk::kTrue)
		{
			Acad::ErrorStatus es = pHatch->erase(false);
			assert(es == Acad::eOk);
		}

		Acad::ErrorStatus es = pHatch->setLayer(this->layerId());
		assert(es == Acad::eOk);

		RbCSDimStylePtr dimStyle(viewport->GetDimStyleId(), AcDb::kForRead);
		if (dimStyle.openStatus() == Acad::eOk)
		{
			Acad::ErrorStatus es = pHatch->setPatternAngle(
					dimStyle->GetHatchAngle());
			//assert(es == Acad::eOk);

			es = pHatch->setPatternScale(dimStyle->GetHatchScale());
			//assert(es == Acad::eOk);

			es = pHatch->setPattern(AcDbHatch::kPreDefined,
					dimStyle->GetHatchPattern().c_str());
			//assert(es == Acad::eOk);

			es = pHatch->setHatchStyle(AcDbHatch::kNormal);
			//assert(es == Acad::eOk);

			es = pHatch->setVisibility(dimStyle->GetCutHatching() ?
					AcDb::kVisible : AcDb::kInvisible);
			//assert(es == Acad::eOk);
		}

		while (pHatch->numLoops() > 0)
		{
			es = pHatch->removeLoopAt(pHatch->numLoops() - 1);
			assert(es == Acad::eOk);
		}

		for (size_t i = 0; i < myHatch.loopsNumber(); ++i)
		{
			Loops loops;
			this->toShortedPresentation(myHatch.loop(i).points(), loops);

			Loops::const_iterator iter = loops.begin();
			Loops::const_iterator iterEnd = loops.end();
			for (; iter != iterEnd; ++iter)
			{
				const AcGePoint2dArray& points = *iter;

				AcGeDoubleArray bulges(points.length());
				bulges.setAll(0.0);

				Acad::ErrorStatus es = pHatch->appendLoop(AcDbHatch::kExternal,
						points,	bulges);
				assert(es == Acad::eOk);
			}
		}

		es = pHatch->evaluateHatch(true);
		//assert(es == Acad::eOk);

		es = pHatch->transformBy(m_OCS);
		assert(es == Acad::eOk);
	}
}

void RbCSDrawingPresenter::
toShortedPresentation(const AcGePoint2dArray& loop, Loops& loops) const
{
	if (m_Shortenings.empty() == true)
	{
		loops.push_back(loop);

		return;
	}

	double shortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;

	loops.push_back(loop);

	RCAD::TSoftPtrIdVector::const_iterator iter = m_Shortenings.begin();
	RCAD::TSoftPtrIdVector::const_iterator iterEnd = m_Shortenings.end();
	for (; iter != iterEnd; ++iter)
	{
		Loops newLoops;

		AcDbObjectId shorteningId = *iter;
		assert(shorteningId.isValid() == true);

		TDwgShorteningPtr shortening(shorteningId, AcDb::kForRead);

		RbCSDwgShortening::TBlockVector::const_iterator blocksIter =
				shortening->m_Blocks.begin();
		RbCSDwgShortening::TBlockVector::const_iterator blocksEnd =
				shortening->m_Blocks.end();
		for (; blocksIter != blocksEnd; ++blocksIter)
		{
			const RbCSDwgShortening::TBlock& block = *blocksIter;

			AcGeVector2d translation = block.Translation -
					block.Translation.normal() * shortGap;

			AcGePoint2d min, max;
			block.Bounds.getMinMaxPoints(min, max);

			AcGeClipBoundary2d clipBounds(min, max);

			AcGe::ClipCondition clipCondition;
			AcGe::ClipError clipError;
			AcGePoint2dArray clippedPoints;

			Loops::iterator loopsIter = loops.begin();
			Loops::iterator loopsEnd = loops.end();
			for(; loopsIter != loopsEnd; ++loopsIter)
			{
				clipError = clipBounds.clipPolygon(*loopsIter, clippedPoints,
						clipCondition);
				assert(clipError == AcGe::eOk);

				if (clipCondition == AcGe::kSegmentsIntersect)
				{
					for (size_t i = 0; i < clippedPoints.length(); ++i)
						clippedPoints[i].transformBy(translation);

					newLoops.push_back(clippedPoints);
				}
				else if (clipCondition == AcGe::kAllSegmentsInside)
				{
					for (size_t i = 0; i < (*loopsIter).length(); ++i)
						(*loopsIter)[i].transformBy(translation);

					newLoops.push_back(*loopsIter);
				}
			}
		}

		//if (newLoops.empty() == false)
		{
			loops.swap(newLoops);
		}
	}
}

Acad::ErrorStatus RbCSDrawingPresenter::
subErase(Adesk::Boolean erasing)
{
	Acad::ErrorStatus es = this->RbCSDrawingEntity::subErase(erasing);

	if (es == Acad::eOk)
	{
		if (myHatchId.isNull() == false)
		{
			ARX::DbHatchPtr pHatch(myHatchId, AcDb::kForWrite, true);

			if (pHatch.openStatus() == Acad::eOk)
			{
				es = pHatch->erase(erasing);
				assert(es == Acad::eOk);
			}
		}

		UserObjEntIds::iterator iter = myDwgUserObjEntIds.begin();
		UserObjEntIds::iterator iterEnd = myDwgUserObjEntIds.end();
		for (; iter != iterEnd; ++iter)
		{
			DwgUserObjEntityPtr pDwgUserObjEntity(*iter, AcDb::kForWrite,
					true);

			if (pDwgUserObjEntity.openStatus() == Acad::eOk)
			{
				pDwgUserObjEntity->myErasingIsAllowed = true;

				Acad::ErrorStatus es = pDwgUserObjEntity->erase(erasing);
				assert(es == Acad::eOk);

				pDwgUserObjEntity->myErasingIsAllowed = false;
			}
		}
	}

	return es;
}

AcDbObjectId RbCSDrawingPresenter::
hatchId() const
{
	this->assertReadEnabled();

	return myHatchId;
}

void RbCSDrawingPresenter::
addUserObject(const AcDbObjectId& id, const AcGePlane& plane)
{
	this->assertWriteEnabled();

	UserObjEntry entry;

	entry.myObjectId = id;
	entry.myPlane = plane;

	myUserObjEntries.push_back(entry);
}

void RbCSDrawingPresenter::
addDwgUserObjEntityId(const AcDbObjectId& entityId)
{
	this->assertWriteEnabled();

	myDwgUserObjEntIds.push_back(entityId);
}

void RbCSDrawingPresenter::
updateDwgUserObjEntities()
{
	UserObjEntIds::iterator iter = myDwgUserObjEntIds.begin();
	UserObjEntIds::iterator iterEnd = myDwgUserObjEntIds.end();
	for (; iter != iterEnd; ++iter)
	{
		DwgUserObjEntityPtr pDwgUserObjEntity(*iter, AcDb::kForWrite, true);

		if (pDwgUserObjEntity.openStatus() == Acad::eOk)
		{
			if (pDwgUserObjEntity->isErased() == Adesk::kTrue)
			{
				Acad::ErrorStatus es = pDwgUserObjEntity->erase(Adesk::kFalse);
				assert(es == Acad::eOk);
			}

			pDwgUserObjEntity->recordGraphicsModified();
		}
	}
}

void RbCSDrawingPresenter::
convertUserObjEntries()
{
	if (myUserObjEntries.empty() == true)
		return;

	UserObjEntIds dwgUserObjEntIds;

	dwgUserObjEntIds.reserve(myUserObjEntries.size());

	AcDbObjectId layerId = this->layerId();

	AcDbDatabase* pDatabse = this->database();
	assert(pDatabse != 0);

	ARX::DbBlockTablePtr pBlockTable(pDatabse->blockTableId(), AcDb::kForRead);

	AcDbObjectId spaceId;
	pBlockTable->getAt(ACDB_MODEL_SPACE, spaceId);

	ARX::DbBlockTableRecordPtr pSpace(spaceId, AcDb::kForWrite);

	if (pSpace.openStatus() != Acad::eOk)
		return;

	UserObjEntries::iterator iter = myUserObjEntries.begin();
	UserObjEntries::iterator iterEnd = myUserObjEntries.end();
	for (; iter != iterEnd; ++iter)
	{
		DwgUserObjEntityPtr pDwgUserObj;
		Acad::ErrorStatus es = pDwgUserObj.create();
		assert(es == Acad::eOk);

		pDwgUserObj->setUserObjId(iter->myObjectId);

		pDwgUserObj->setPlane(iter->myPlane);

		pDwgUserObj->setView(m_ViewDir, m_UpVector);

		es = pDwgUserObj->setLayer(layerId);
		assert(es == Acad::eOk);

		es = pSpace->appendAcDbEntity(pDwgUserObj.object());
		assert(es == Acad::eOk);

		pDwgUserObj->setDwgPresenterId(this->objectId());

		dwgUserObjEntIds.push_back(pDwgUserObj->objectId());
	}

	myDwgUserObjEntIds.swap(dwgUserObjEntIds);

	UserObjEntries().swap(myUserObjEntries);
}

Acad::ErrorStatus RbCSDrawingPresenter::
writeDwgUserObjEntIds(AcDbDwgFiler* pFiler) const
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	Acad::ErrorStatus es = pFiler->writeUInt32(myDwgUserObjEntIds.size());
	assert(es == Acad::eOk);

	UserObjEntIds::const_iterator iter = myDwgUserObjEntIds.begin();
	UserObjEntIds::const_iterator iterEnd = myDwgUserObjEntIds.end();
	for (; iter != iterEnd; ++iter)
	{
		es = pFiler->writeSoftPointerId(*iter);
		assert(es == Acad::eOk);
	}

	return pFiler->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
readDwgUserObjEntIds(AcDbDwgFiler* pFiler)
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	Adesk::UInt32 size = 0;
	Acad::ErrorStatus es = pFiler->readUInt32(&size);
	assert(es == Acad::eOk);

	UserObjEntIds dwgUserObjEntIds;
	dwgUserObjEntIds.reserve(size);

	for (Adesk::UInt32 i = 0; i < size; ++i)
	{
		AcDbSoftPointerId id;
		es = pFiler->readSoftPointerId(&id);
		assert(es == Acad::eOk);

		dwgUserObjEntIds.push_back(id);
	}

	myDwgUserObjEntIds.swap(dwgUserObjEntIds);

	return pFiler->filerStatus();
}

#include <vector>

namespace RbCS
{

namespace Private
{

struct GeObjectData
{
	Adesk::UInt16 myColorIndex;
	bool meIsHidden;
	AcDbHandle myLineTypeHandle;
	AcDbHandle myLinkedObjHandle;
	unsigned long myMachiningId;
	AcGePoint3d myNodePoint;
	unsigned long myTag;
	bool meIsVisible;
	AcGe::EntityId myGeType;
	AcGePoint3d myStartPoint;
	AcGePoint3d myEndPoint;
};

typedef std::vector<GeObjectData> GeObjectDataVector;

}

}


/*
Acad::ErrorStatus RbCSDrawingPresenter::
writeGraphObjSet_small_blocks( AcDbDwgFiler* pFiler ) const
{
	assert( pFiler != 0 );
	assert( pFiler->filerStatus() == Acad::eOk );

	if ( pFiler->filerType() != AcDb::kFileFiler )
	{
		return this->SaveGraphObjSet2( pFiler );
	}

	Acad::ErrorStatus es = Acad::eOk;

	RCAD::CGeDataCollection2::const_iterator objIter = m_aGeObjects.Begin();
	RCAD::CGeDataCollection2::const_iterator objIterEnd = m_aGeObjects.End();

	const size_t objectsInBlockCount = 10000; // an empiric value
	const size_t objectsCount = m_aGeObjects.GetSize();

	const Adesk::UInt32 blocksCount = objectsCount / objectsInBlockCount +1;
	es = pFiler->writeUInt32( blocksCount );
	assert( es == Acad::eOk );

	for ( size_t blockIndex = 0; blockIndex < blocksCount; ++blockIndex )
	{
		RbCS::Private::GeObjectDataVector geObjDataVector;
		geObjDataVector.reserve( objectsInBlockCount );

		for ( size_t objInBlockIndex = 0 ; 
			  objIter != objIterEnd && objInBlockIndex < objectsInBlockCount; 
			  ++objIter, ++objInBlockIndex )
		{
			RCAD::TStdGeObject GeObject = *objIter;

			AcGeLineSeg3d* pLineSeg= reinterpret_cast< AcGeLineSeg3d* >(
				GeObject.AsEntity3d() );
			assert( pLineSeg != 0 );

			RbCS::Private::GeObjectData geObjData;

			geObjData.myColorIndex = GeObject.GetColor();
			geObjData.meIsHidden = GeObject.GetIsHidden();
			geObjData.myLineTypeHandle = GeObject.GetLineTypeId().handle();
			geObjData.myLinkedObjHandle = GeObject.GetLinkedObjectId().handle();
			geObjData.myMachiningId = GeObject.machiningId();
			geObjData.myNodePoint = GeObject.GetNodePoint();
			geObjData.myTag = GeObject.GetTag();
			geObjData.meIsVisible = GeObject.IsVisible();
			geObjData.myGeType = GeObject.GetGeType();
			geObjData.myStartPoint = pLineSeg->startPoint();
			geObjData.myEndPoint = pLineSeg->endPoint();

			geObjDataVector.push_back( geObjData );
		}

		Adesk::UInt32 originalSize = geObjDataVector.size() *
									 sizeof(RbCS::Private::GeObjectData);

		uLongf compressedSize = originalSize + originalSize / 4 + 12;

		Bytef* compressedBlock = new Bytef[compressedSize];

#if defined(_ACAD2004)
		int rc = RBCTcompress2( compressedBlock, &compressedSize,
			reinterpret_cast< const Bytef* >( &geObjDataVector[ 0 ] ),
			originalSize, Z_BEST_COMPRESSION );
#else
		int rc = compress2( compressedBlock, &compressedSize,
			reinterpret_cast< const Bytef* >( &geObjDataVector[ 0 ] ),
			originalSize, Z_BEST_COMPRESSION );
#endif
		assert( rc == Z_OK );
		
		es = pFiler->writeUInt32( originalSize );
		assert(es == Acad::eOk);

		es = pFiler->writeUInt32( compressedSize );
		assert( es == Acad::eOk );

		es = pFiler->writeBytes( compressedBlock, compressedSize );
		assert( es == Acad::eOk );

		delete[] compressedBlock;
	}

	return pFiler->filerStatus();
}


Acad::ErrorStatus RbCSDrawingPresenter::
writeGraphObjSet(AcDbDwgFiler* pFiler) const
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	if (pFiler->filerType() != AcDb::kFileFiler)
		return this->SaveGraphObjSet2(pFiler);

	RbCS::Private::GeObjectDataVector geObjDataVector;

	geObjDataVector.reserve(m_aGeObjects.GetSize());

	RCAD::CGeDataCollection2::const_iterator iter = m_aGeObjects.Begin();
	RCAD::CGeDataCollection2::const_iterator iterEnd = m_aGeObjects.End();
	for (; iter != iterEnd; ++iter)
	{
		RCAD::TStdGeObject GeObject = *iter;

		AcGeLineSeg3d* pLineSeg= reinterpret_cast<AcGeLineSeg3d*>(
				GeObject.AsEntity3d());
		assert(pLineSeg != 0);

		RbCS::Private::GeObjectData geObjData;

		// KSA {
		//geObjData.myColorIndex = pGeObject->GetColor().colorIndex();
		geObjData.myColorIndex = GeObject.GetColor();
		// }
		geObjData.meIsHidden = GeObject.GetIsHidden();
		geObjData.myLineTypeHandle = GeObject.GetLineTypeId().handle();
		geObjData.myLinkedObjHandle = GeObject.GetLinkedObjectId().handle();
		geObjData.myMachiningId = GeObject.machiningId();
		geObjData.myNodePoint = GeObject.GetNodePoint();
		geObjData.myTag = GeObject.GetTag();
		geObjData.meIsVisible = GeObject.IsVisible();
		geObjData.myGeType = GeObject.GetGeType();
		geObjData.myStartPoint = pLineSeg->startPoint();
		geObjData.myEndPoint = pLineSeg->endPoint();

		geObjDataVector.push_back(geObjData);
	}

	size_t originalSize = geObjDataVector.size() *
			sizeof(RbCS::Private::GeObjectData);

	uLongf compressedSize = originalSize + originalSize / 4 + 12;

	Bytef* compressedBlock = new Bytef[compressedSize];

#if defined(_ACAD2004)
	int rc = RBCTcompress2(compressedBlock, &compressedSize,
			reinterpret_cast<const Bytef*>(&geObjDataVector[0]),
			originalSize, Z_BEST_COMPRESSION);
#else
	int rc = compress2(compressedBlock, &compressedSize,
			reinterpret_cast<const Bytef*>(&geObjDataVector[0]),
			originalSize, Z_BEST_COMPRESSION);
#endif
	assert(rc == Z_OK);

	Acad::ErrorStatus es = Acad::eOk;

	es = pFiler->writeUInt32(originalSize);
	assert(es == Acad::eOk);

	es = pFiler->writeUInt32(compressedSize);
	assert(es == Acad::eOk);

	es = pFiler->writeBytes(compressedBlock, compressedSize);
	assert(es == Acad::eOk);

	delete[] compressedBlock;

	return pFiler->filerStatus();
}*/

Acad::ErrorStatus RbCSDrawingPresenter::
readGraphObjSet_small_blocks( AcDbDwgFiler* pFiler )
{
	assert( pFiler != 0 );
	assert( pFiler->filerStatus() == Acad::eOk );

	if ( pFiler->filerType() != AcDb::kFileFiler )
		return this->LoadGraphObjSet2( pFiler );

	Acad::ErrorStatus es = Acad::eOk;

	AcDbDatabase* pDatabase = this->database();
	assert(pDatabase != 0);

	Adesk::UInt32 blocksCount = 0;
	es = pFiler->readUInt32( &blocksCount );
	assert( es == Acad::eOk );

	RCAD::CGeDataCollection2::WriterPtr wr = m_aGeObjects.GetWriter();
	if ( wr->IsValid() == false )
		return Acad::eMakeMeProxy;

	for ( size_t blockIndex = 0; blockIndex < blocksCount; ++blockIndex )
	{
		Adesk::UInt32 originalSize = 0;
		es = pFiler->readUInt32( &originalSize );
		assert( es == Acad::eOk );

		Adesk::UInt32 compressedSize = 0;
		es = pFiler->readUInt32( &compressedSize );
		assert( es == Acad::eOk );

		Bytef* compressedBlock = new Bytef[ compressedSize ];

		es = pFiler->readBytes( compressedBlock, compressedSize );
		assert( es == Acad::eOk );

		RbCS::Private::GeObjectDataVector geObjDataVector;

		geObjDataVector.resize( originalSize / sizeof( RbCS::Private::GeObjectData ) );

		#if defined( _ACAD2004 )
			int rc = RBCTuncompress( reinterpret_cast< Bytef* >( &geObjDataVector[ 0 ] ),
								     &originalSize, compressedBlock, compressedSize );
		#else
			int rc = uncompress( reinterpret_cast< Bytef* >( &geObjDataVector[ 0 ] ),
				                 &originalSize, compressedBlock, compressedSize );
		#endif
			assert( rc == Z_OK );

		delete[] compressedBlock;

		RbCS::Private::GeObjectDataVector::iterator iter = geObjDataVector.begin();
		RbCS::Private::GeObjectDataVector::iterator iterEnd = geObjDataVector.end();

		AcDbObjectId lineTypeId;
		AcDbObjectId linkedObjectId;

		for (; iter != iterEnd; ++iter)
		{
			RbCS::CGeObject GeObject;

			es = pDatabase->getAcDbObjectId(lineTypeId, false,
				iter->myLineTypeHandle);

			es = pDatabase->getAcDbObjectId(linkedObjectId, false,
				iter->myLinkedObjHandle);

			GeObject.SetPoints3d( iter->myStartPoint, iter->myEndPoint );
			//GeObject.SetColorIndex( iter->myColorIndex );
			GeObject.SetHidden( iter->meIsHidden );
			//GeObject.SetLineTypeId( lineTypeId );
			GeObject.SetLinkedObjId( linkedObjectId );
			GeObject.SetMachId( ( unsigned short ) iter->myMachiningId );
			//GeObject.SetNodePoint( iter->myNodePoint );
			GeObject.SetTag( ( unsigned short ) iter->myTag );
			GeObject.SetVisible( iter->meIsVisible );

			wr->Append() = GeObject;
		}
	}

	return pFiler->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
readGraphObjSet(AcDbDwgFiler* pFiler)
{
	assert(pFiler != 0);
	assert(pFiler->filerStatus() == Acad::eOk);

	RCAD::CGeDataCollection2::WriterPtr wr = m_aGeObjects.GetWriter();
	if ( wr->IsValid() == false )
		return Acad::eMakeMeProxy;

	if (pFiler->filerType() != AcDb::kFileFiler)
		return this->LoadGraphObjSet2(pFiler);

	Acad::ErrorStatus es = Acad::eOk;

	Adesk::UInt32 originalSize = 0;
	es = pFiler->readUInt32(&originalSize);
	assert(es == Acad::eOk);

	Adesk::UInt32 compressedSize = 0;
	es = pFiler->readUInt32(&compressedSize);
	assert(es == Acad::eOk);

	Bytef* compressedBlock = new Bytef[compressedSize];

	es = pFiler->readBytes(compressedBlock, compressedSize);
	assert(es == Acad::eOk);

	RbCS::Private::GeObjectDataVector geObjDataVector;

	geObjDataVector.resize(originalSize / sizeof(RbCS::Private::GeObjectData));

#if defined(_ACAD2004)
	int rc = RBCTuncompress(reinterpret_cast<Bytef*>(&geObjDataVector[0]),
			&originalSize, compressedBlock, compressedSize);
#else
	int rc = uncompress(reinterpret_cast<Bytef*>(&geObjDataVector[0]),
			&originalSize, compressedBlock, compressedSize);
#endif
	assert(rc == Z_OK);

	delete[] compressedBlock;

	RbCS::Private::GeObjectDataVector::iterator iter = geObjDataVector.begin();
	RbCS::Private::GeObjectDataVector::iterator iterEnd = geObjDataVector.end();

	AcDbObjectId lineTypeId;
	AcDbObjectId linkedObjectId;

	AcDbDatabase* pDatabase = this->database();
	assert(pDatabase != 0);

	for (; iter != iterEnd; ++iter)
	{
		RbCS::CGeObject GeObject;

		es = pDatabase->getAcDbObjectId(lineTypeId, false,
				iter->myLineTypeHandle);

		es = pDatabase->getAcDbObjectId(linkedObjectId, false,
				iter->myLinkedObjHandle);

		//GeObject.SetColorIndex( iter->myColorIndex );
		GeObject.SetPoints3d( iter->myStartPoint, iter->myEndPoint );
		GeObject.SetHidden( iter->meIsHidden );
		//GeObject.SetLineTypeId( lineTypeId );
		GeObject.SetLinkedObjId( linkedObjectId );
		GeObject.SetMachId( ( unsigned short ) iter->myMachiningId );
		//GeObject.SetNodePoint(iter->myNodePoint);
		GeObject.SetTag( ( unsigned short ) iter->myTag );
		GeObject.SetVisible( iter->meIsVisible );
		
		wr->Append() = GeObject;
	}

	return pFiler->filerStatus();
}

Acad::ErrorStatus RbCSDrawingPresenter::
subClose()
{
	/*if (this->isWriteEnabled() == Adesk::kTrue)
	{
		if (m_uReadedVersion == 8)
		{
			this->convertUserObjEntries();
		}
	}*/

	return this->RbCSDrawingEntity::subClose();
}

Acad::ErrorStatus RbCSDrawingPresenter::
subIntersectWith(const AcDbEntity* pEnt, AcDb::Intersect intType,
		AcGePoint3dArray& points, int thisGsMarker, int otherGsMarker) const
{
	return Acad::eNotImplementedYet;
}

Acad::ErrorStatus RbCSDrawingPresenter::
subIntersectWith(const AcDbEntity* pEnt, AcDb::Intersect intType,
		const AcGePlane& projPlane, AcGePoint3dArray& points, int thisGsMarker,
		int otherGsMarker) const
{
	return Acad::eNotImplementedYet;
}

void RbCSDrawingPresenter::
ClearCompressorCashes()
{
	m_WR = RCAD::CGeDataCollection2::WriterPtr(0);
	m_drCashWR = RCAD::CGeDataCollection::WriterPtr(0);
}

void RbCSDrawingPresenter::
GlobalEnableSwapping( bool bEnable )
{
	s_bEnableSwapping = bEnable;
}

bool RbCSDrawingPresenter::
GetGlobalSwappingState()
{
	return s_bEnableSwapping;
}

void RbCSDrawingPresenter::
GlobalEnableSmoothArcs( bool b )
{
	s_bSmoothArcs = b;
}

bool RbCSDrawingPresenter::
GetGlobalSmoothArcsState()
{
	return s_bSmoothArcs;
}

void RbCSDrawingPresenter::
GlobalEnableImprove( bool b )
{
	s_bUseIntImprove = b;
}

bool RbCSDrawingPresenter::
GetGlobalUsingImproveState()
{
	return s_bUseIntImprove;
}

bool RbCSDrawingPresenter::
IsValid() const
{
	return m_aGeObjects.IsValid();
}

// cashing external contour
void RbCSDrawingPresenter::SetExternalContour( const AcGePolyline2d& externalContour )
{
	assertWriteEnabled(true); 

	m_externalContour = externalContour;
}
AcGePolyline2d RbCSDrawingPresenter::GetExternalContour() const
{
	assertReadEnabled();

	return m_externalContour;
}

void RbCSDrawingPresenter::AddCircle( RbCS::CGeArc& circ )
{
	if ( !circ.IsHidden() && ( circ.GetMachId() & RbCTBody::eTypeMask ) != RbCTBody::eDrilling )
	{
		m_circs.push_back( circ );
		return;
	}

	RCAD::CGeArcCollection::iterator iter = m_circs.begin();
	RCAD::CGeArcCollection::iterator iter_end = m_circs.end();

	bool foundEqual = false;
	bool changeVisib = false;
	RCAD::CGeArcCollection::iterator iterChanged = m_circs.end();

	for ( ; iter != iter_end; ++iter )
	{
		if ( iter->isGeomEqualTo( circ, algo::kTol ) )
		{
			foundEqual = true;
			if ( ( iter->IsHidden() && !circ.IsHidden() ) || ( !iter->IsVisible() && circ.IsVisible() ) )
			{
				*iter = circ;
				changeVisib = true;
				iterChanged = iter;
			}
			break;
		}
	}

	if ( changeVisib )
	{
		iter = m_circs.begin();
		for ( ; iter != m_circs.end(); )
		{
			if ( iter != iterChanged && iter->isOverlapped( circ, algo::kTol ) )
			{
				iter = m_circs.erase( iter );
			}
			else
			{
				++iter;
			}
		}		
	}

	if ( !foundEqual )
	{
		iter = m_circs.begin();

		bool circleIsOverlapped = false;

		for ( ; iter != iter_end; ++iter )
		{
			if ( circ.isOverlapped( *iter, algo::kTol ) )
			{
				circleIsOverlapped = true;
				break;
			}
		}

		if ( !circleIsOverlapped )
		{
			iter = m_circs.begin();
			for ( ; iter != m_circs.end(); )
			{
				if ( iter->isOverlapped( circ, algo::kTol ) )
				{
					iter = m_circs.erase( iter );
				}
				else
				{
					++iter;
				}
			}
			m_circs.push_back( circ );
		}
	}
}

const RCAD::CGeArcCollection& RbCSDrawingPresenter::GetOrigGeArcSet() const
{
	return m_circs;
}

RCAD::CGeArcCollection& RbCSDrawingPresenter::GetDispGeArcSet(  RCAD::CGeArcCollection& origSet, bool useShortennings, bool useClipping ) const
{
	double ShortGap = RbCSDwgShortening::GetShortGap(this->ViewportId) / 2.0;
	assert(ShortGap >= 0.0);

	if ( Is3dView() || !( useShortennings || useClipping ) )
		return origSet;

	/*if (!RbCSDrawingPresenter::bUseIntImprove)
	{
		origSet.SetProjectedComponents( AcGePlane(AcGePoint3d::kOrigin,
			m_ViewDir.crossProduct(m_UpVector), m_UpVector), - m_ViewDir );
	}
	else */
	{
		origSet.SetProjectedComponents( AcGePlane(AcGePoint3d::kOrigin,
			AcGeVector3d::kXAxis, AcGeVector3d::kYAxis), AcGeVector3d::kZAxis);
	}

	if (useShortennings == true)
		ApplyShortenings<RCAD::CGeArcCollection,AcGeBoundBlock2d>( *this, m_Shortenings, origSet );

	if ((useClipping == true) && (m_IsClipped == true))
		ApplyClipping<RCAD::CGeArcCollection,AcGeBoundBlock2d>( *this, origSet );

	return origSet;
}

Acad::ErrorStatus RbCSDrawingPresenter::explodeArcs(AcDbVoidPtrArray& EntitySet) const
{
	CLineInfo LineInfo;

	AcDbObjectPointer<RbCSViewport> Viewport(m_ViewportId, AcDb::kForRead);

	if (Viewport.openStatus() == Acad::eOk)
		Viewport->GetHiddenLineInfo(LineInfo);

	// Painted User Parts
	bool showUserParts = true;
	AcDbObjectId userPartsLineTypeId;
	Adesk::UInt16 userPartsLineColour = 0;
	AcDb::LineWeight userPartsLineWeight = AcDb::kLnWt000;
	//

	// Painted Machining
	bool machPresentation = false;
	AcDbObjectId machPartsLineTypeId;
	Adesk::UInt16 machPartsLineColour = 0;
	AcDb::LineWeight machPartsLineWeight = AcDb::kLnWt000;
	//
	{RbCSDimStylePtr dimStyle(Viewport->GetDimStyleId(), AcDb::kForRead);
	if (dimStyle.openStatus() == Acad::eOk)
	{
		showUserParts = dimStyle->getUserParts();
		userPartsLineTypeId = dimStyle->getUserPartsLineTypeId();
		userPartsLineColour = dimStyle->getUserPartsLineColour();
		userPartsLineWeight = dimStyle->getUserPartsLineThickness();
	}}
	{RbCSSinglePartDimStylePtr dimStyleSP( Viewport->GetDimStyleId(), AcDb::kForRead );
	if ( dimStyleSP.openStatus() == Acad::eOk )
	{
		machPresentation = dimStyleSP->getMachiningPresentation() == RbCSSinglePartDimStyle::mpDistinct;
		machPartsLineTypeId = dimStyleSP->getMachiningLineType();
		machPartsLineColour = dimStyleSP->getMachiningLineColour();
		machPartsLineWeight = dimStyleSP->getMachiningLineWeight();
	}}

	AcGeMatrix3d Transf = m_OCS;

	/*
	if ((this->Is3dView() == true) && (!RbCSDrawingPresenter::bUseIntImprove))
	{
		AcGeMatrix3d transf = AcGeMatrix3d::alignCoordSys(
			AcGePoint3d::kOrigin, m_UpVector.crossProduct(-m_ViewDir),
			m_UpVector,-m_ViewDir, AcGePoint3d::kOrigin,
			AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
			AcGeVector3d::kZAxis);

		Transf *= transf;
	}
	*/

	{AcDbObjectPointer< RbCSViewport > Viewport( m_ViewportId, AcDb::kForRead);
	AcGeMatrix3d mtrx = Transf;
	/*
	if (( Viewport.openStatus() == Acad::eOk) && (this->Is3dView() != true) && (!RbCSDrawingPresenter::bUseIntImprove))
	{
		AcGeMatrix3d tmpMatr;
		AcGeVector3d vDir, uDir;
		Viewport->getProjComponents( tmpMatr, vDir, uDir );
		tmpMatr.entry[0][3] = 0.0;
		tmpMatr.entry[1][3] = 0.0;
		tmpMatr.entry[2][3] = 0.0;
		mtrx = mtrx.postMultBy( tmpMatr );
	}
	*/

	RCAD::CGeArcCollection arc_set = GetOrigGeArcSet();
	GetDispGeArcSet( arc_set, true, true );

	RbCS::GeArcsArray::const_iterator it_circ = arc_set.begin();
	RbCS::GeArcsArray::const_iterator it_circ_end = arc_set.end();

	for ( ; it_circ != it_circ_end; ++it_circ )
	{
		const RbCS::CGeArc& geObject = *it_circ;

		if ( geObject.IsVisible() == false )
			continue;

		if ( ( geObject.IsHidden() == true ) && ( LineInfo.m_bVisible == false ) )
			continue;

		if ( ( geObject.GetTag() == RCAD::stGrate || geObject.GetTag() == RCAD::stUserObj ) &&
			( showUserParts == false ) )
			continue;

		AcDbEntity* pNewEntity = 0;
		//
		if ( it_circ->IsKindOf( AcGe::kCircArc3d ) )
		{
			RbCS::TEntity3dPtr ent = it_circ->AsEntity3d();
			AcGeCircArc3d* arc = static_cast< AcGeCircArc3d* >( ent.get() );

			if ( !arc->isClosed() )
			{
				double stAngle = arc->startAng();
				double enAngle = arc->endAng();
				AcGePoint3d center = arc->center();
				AcGeVector3d normal = arc->normal();
				double radius = arc->radius();
				AcDbArc* newArc = new AcDbArc(
					center,
					normal,
					radius,
					stAngle,
					enAngle );
				assert( newArc != 0 );

				AcGePoint3d arcStPnt = arc->startPoint();
				AcGePoint3d arcDbStPnt;
				newArc->getStartPoint( arcDbStPnt );

				AcGePoint3d arcEnPnt = arc->endPoint();
				AcGePoint3d arcDbEnPnt;
				newArc->getEndPoint( arcDbEnPnt );

				AcGeVector3d arcVectSt = arcStPnt - center;
				AcGeVector3d arcDbVectSt = arcDbStPnt - center;

				AcGePlane plane( center, normal );
				AcGeVector2d stVect2d = arcVectSt.convert2d( plane );
				AcGeVector2d stDbVect2d = arcDbVectSt.convert2d( plane );
				double angle = stVect2d.angleTo( stDbVect2d );
				angle = stVect2d.perpVector().dotProduct( stDbVect2d ) > 0 ? angle : M_2PI - angle;

				AcGeMatrix3d rotMtrx = AcGeMatrix3d::rotation( -angle, normal, center );
				newArc->transformBy( rotMtrx );

				pNewEntity = newArc;
			}
			else
			{
				pNewEntity = new AcDbCircle(
					arc->center(),
					arc->normal(),
					arc->radius());
				assert( pNewEntity != 0 );
			}
		}
		else
		{
			assert( false );
		}
		if ( pNewEntity != 0 )
		{
			pNewEntity->transformBy( mtrx );
			pNewEntity->setPropertiesFrom( this );

			if ( geObject.IsHidden() )
			{
				pNewEntity->setColorIndex( LineInfo.m_iColorIndex );
				pNewEntity->setLinetype( LineInfo.m_LineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( LineInfo.m_iLineWeight );
			}
			else if ( geObject.GetTag() == RCAD::stGrate || geObject.GetTag() == RCAD::stUserObj )
			{
				pNewEntity->setColorIndex( userPartsLineColour );
				pNewEntity->setLinetype( userPartsLineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( userPartsLineWeight );
			}
			else if ( machPresentation && ( geObject.GetTag() == RCAD::stDrillingStraight ||
				geObject.GetTag() == RCAD::stDrillingCircle || geObject.GetTag() == RCAD::stMachining ) )
			{
				pNewEntity->setColorIndex( machPartsLineColour );
				pNewEntity->setLinetype( machPartsLineTypeId );
				pNewEntity->setLinetypeScale( LineInfo.m_dLineTypeScale );
				pNewEntity->setLineWeight( machPartsLineWeight );
			}

			EntitySet.append( pNewEntity );
		}
	}}
	return Acad::eOk;
}

void RbCSDrawingPresenter::
CashGeObjects( const RCAD::TGeSet& geSet, bool bAppend )
{
	if ( bAppend )
	{
		m_geCash.insert( m_geCash.end(), geSet.begin(), geSet.end() );
	}
	else
	{
		m_geCash.assign( geSet.begin(), geSet.end() );
	}
}

void RbCSDrawingPresenter::
GetCashedGeObjects( RCAD::TGeCash& outStore, bool bClear )
{
	if ( bClear )
	{
		outStore.swap( m_geCash );
		m_geCash.clear();
	}
	else
	{
		outStore.assign( m_geCash.begin(), m_geCash.end() );
	}
}

bool RbCSDrawingPresenter::
HasGeCash() const
{
	return m_geCash.empty() == false;
}

bool RbCSDrawingPresenter::GetDrillingGeometry( AcDbObjectId partId, const RbCSDrilling* pDril,
						 RCAD::CGeArcCollection& arcs, RCAD::CGeDataCollection& segs ) const
{
	AcDbReader< RbCSViewport > Viewport( m_ViewportId );
	if ( !Viewport )
	{
		return false;
	}

	AcGeMatrix3d mtrx = m_OCS;
	/*
	if ( ( this->Is3dView() != true ) && ( !RbCSDrawingPresenter::bUseIntImprove ) )
	{
		AcGeMatrix3d tmpMatr;
		AcGeVector3d vDir, uDir;
		Viewport->getProjComponents( tmpMatr, vDir, uDir );
		tmpMatr.entry[0][3] = 0.0;
		tmpMatr.entry[1][3] = 0.0;
		tmpMatr.entry[2][3] = 0.0;
		mtrx = mtrx.postMultBy( tmpMatr );
	}
	*/
	arcs = GetDrillGeArcSet( partId, *pDril );
	GetDispGeArcSet( arcs, true, true );

	RCAD::CGeArcCollection::iterator iter = arcs.begin();
	RCAD::CGeArcCollection::iterator iter_end = arcs.end();
	for ( ; iter != iter_end; ++iter )
	{
		iter->transformBy( mtrx );
	}

	segs = GetDrillGeSet( partId, pDril );
	if ( Is3dView() == false )
	{
		GetDrillDispGeSet( segs, true, true );
	}

	return true;
}

RCAD::CGeDataCollection RbCSDrawingPresenter::GetDrillGeSet( const AcDbObjectId& partId, const RbCSDrilling* drill ) const
{
	if ( partId.isNull() )
	{
		return m_aDrillSegCash;
	}

	AcDbReader< RbCSDOPart > pPart( partId );
	int machInd = pPart->GetMachiningIndex( drill ) + 1;

	RCAD::CGeDataCollection segs;
	RCAD::CGeDataCollection::WriterPtr WR = segs.GetWriter();

	RCAD::CGeDataCollection::ReaderPtr rd = m_aDrillSegCash.GetReader();	
	size_t size = rd->GetSize();
	unsigned i;
	for ( i = 0; i < size; ++i)
	{
		const RbCS::CGeObject& obj = (*rd)[i];
		AcDbObjectId objId = obj.GetLinkedObjId();
		if ( objId != partId )
		{
			continue;
		}
		unsigned int machId = obj.GetMachId();
		if ( ( machId & RbCTBody::eTypeMask ) == RbCTBody::eDrilling &&
			( machId & RbCTBody::eIndexMask ) == machInd )
		{
			WR->Append() = obj;
		}
	}
	return segs;
}

RCAD::CGeArcCollection RbCSDrawingPresenter::GetDrillGeArcSet( const AcDbObjectId& partId, const RbCSDrilling& drill ) const
{
	if ( partId.isNull() )
	{
		return GetOrigGeArcSet();
	}

	AcDbReader< RbCSDOPart > pPart( partId );
	int machInd = pPart->GetMachiningIndex( &drill ) + 1;

	RCAD::CGeArcCollection arcs;

	RCAD::CGeArcCollection::const_iterator iter = m_circs.begin();
	RCAD::CGeArcCollection::const_iterator iter_end = m_circs.end();

	for ( ; iter != iter_end; ++iter )
	{
		const RbCS::CGeArc& arc = *iter;
		AcDbObjectId objId = arc.GetLinkedObjId();
		if ( objId != partId )
		{
			continue;
		}
		unsigned int machId = arc.GetMachId();
		if ( ( machId & RbCTBody::eTypeMask ) == RbCTBody::eDrilling &&
			( machId & RbCTBody::eIndexMask ) == machInd )
		{
			arcs.push_back( arc );
		}
	}
	return arcs;
}

void RbCSDrawingPresenter::AddSegToDrillCash( const RbCS::CGeObject& seg )
{
	if ( m_drCashWR.get() == 0 )
	{
		m_drCashWR = m_aDrillSegCash.GetWriter();
	}

	m_drCashWR->Append() = seg;
}
void RbCSDrawingPresenter::
SetReadingTransform( const AcGeMatrix3d& mx )
{
	m_mxRdTransform = mx;
}

CGeDataAccessor::
CGeDataAccessor()
{
	//m_VpData.mUseClip = false;
	//m_VpData.mUseShorts = false;
}


bool RbCSDrawingPresenter::GetAllSnapPoints( RCAD::SetPnts2d& setPnts ) const
{
	using namespace RbCS;

	if ( IsValid() == false )
	{
		return false;
	}

	CLineInfo LineInfo;
	bool showUserParts = true;
	bool machPresentation = false;

	AcGeMatrix3d transf = m_OCS;

	{
	AcDbObjectPointer<RbCSViewport> Viewport(m_ViewportId,
			AcDb::kForRead);
	if (Viewport.openStatus() != Acad::eOk)
		return false;

	Viewport->GetHiddenLineInfo(LineInfo);

	{RbCSDimStylePtr dimStyle(Viewport->GetDimStyleId(), AcDb::kForRead);
	if (dimStyle.openStatus() == Acad::eOk)
	{
		showUserParts = dimStyle->getUserParts();
	}}
	/*
	if ((this->Is3dView() == true) && (!RbCSDrawingPresenter::bUseIntImprove))
	{
		AcGeMatrix3d Transf = AcGeMatrix3d::alignCoordSys(
				AcGePoint3d::kOrigin, m_UpVector.crossProduct(-m_ViewDir),
				m_UpVector,	-m_ViewDir, AcGePoint3d::kOrigin,
				AcGeVector3d::kXAxis, AcGeVector3d::kYAxis,
				AcGeVector3d::kZAxis);
		transf *= Transf;
	}
	*/

	RCAD::CGeDataCollection2 displayed_set = CGeDataAccessor::Instance()->GetDispGeSet( *this, true, true );

	RCAD::CGeDataCollection2::ReaderPtr rd = displayed_set.GetReader();	
	size_t size = rd->GetSize();
	unsigned i;
	for ( i = 0; i < size; ++i)
	{
		const CGeObject2d& obj = (*rd)[i];

		if ( obj.IsVisible() == false )
			continue;

		if ( obj.IsHidden() == true )
		{
			if ( ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj ) &&
				( showUserParts == false ) )
				continue;

			if ( LineInfo.m_bVisible == false )
				continue;
		}
		else
		{
			if ( obj.GetTag() == RCAD::stGrate || obj.GetTag() == RCAD::stUserObj )
			{
				if (showUserParts == false)
					continue;
			}
		}

		if ( obj.GetTag() == RCAD::stBreakLine )
		{
			continue;
		}

		switch ( obj.GetGeType() )
		{
			case AcGe::kLineSeg2d:
			{
				TEntity2dPtr e2d = obj.AsEntity2d();
				const AcGeLineSeg2d* LineSeg =
						static_cast<const AcGeLineSeg2d*>( e2d.get() );

				AcGePoint2d StartPoint = LineSeg->startPoint();
				AcGePoint2d EndPoint = LineSeg->endPoint();

				AcGePoint3d Points[2];
				Points[0] = AcGePoint3d(StartPoint.x, StartPoint.y, 0.0);
				Points[1] = AcGePoint3d(EndPoint.x, EndPoint.y, 0.0);

				Points[0].transformBy(transf);
				Points[1].transformBy(transf);

				setPnts.insert( AcGePoint2d( Points[0].x, Points[0].y ) );
				setPnts.insert( AcGePoint2d( Points[1].x, Points[1].y ) );

				break;
			}
		}
	}
	}
	RCAD::CGeDataCollection segs = GetDrillGeSet( AcDbObjectId::kNull, NULL );
	if ( Is3dView() == false )
	{
		GetDrillDispGeSet( segs, true, true );
	}

	RCAD::CGeDataCollection::ReaderPtr rd = segs.GetReader();
	size_t size = rd->GetSize();

	if ( size > 0 )
	{
		for ( unsigned int i = 0; i < size; ++i )
		{
			const RbCS::CGeObject& obj = ( *rd )[ i ];

			switch ( obj.GetGeType() )
			{
				case AcGe::kLineSeg3d:
				{
					TEntity3dPtr e3d = obj.AsEntity3d();
					const AcGeLineSeg3d* LineSeg =
							static_cast<const AcGeLineSeg3d*>( e3d.get() );

					AcGePoint3d StartPoint = LineSeg->startPoint();
					AcGePoint3d EndPoint = LineSeg->endPoint();

					StartPoint.transformBy(transf);
					EndPoint.transformBy(transf);

					setPnts.insert( AcGePoint2d( StartPoint.x, StartPoint.y ) );
					setPnts.insert( AcGePoint2d( EndPoint.x, EndPoint.y ) );

					break;
				}
			}
		}
	}

	AcDbExtents ext;
	subGetGeomExtents( ext );

	AcGePoint3d maxPnt = ext.maxPoint(), minPnt = ext.minPoint();
	AcGePoint2d
		leftBottom( minPnt.x, minPnt.y ),
		leftTop( minPnt.x, maxPnt.y ),
		rightTop( maxPnt.x, maxPnt.y ),
		rightBottom( maxPnt.x, minPnt.y );

	setPnts.insert( leftBottom );
	setPnts.insert( leftTop );
	setPnts.insert( rightTop );
	setPnts.insert( rightBottom );

	return true;
}


RCAD::CGeDataCollection2 CGeDataAccessor::
GetDispGeSet( const RbCSDrawingPresenter& presenter, bool bUseShortenings, bool bUseClipping )
{
	/*if ( presenter.objectId() != m_VpData.mPresId ||
		bUseShortenings != m_VpData.mUseShorts ||
		bUseClipping != m_VpData.mUseClip ||
		*presenter.GetOrigGeSet().GetToken() != *m_VpData.mToken.get() )
	{
		m_VpData.mPresId = presenter.objectId();
		m_VpData.mUseShorts = bUseShortenings;
		m_VpData.mUseClip = bUseClipping;
		m_VpData.mGeData = presenter.GetDispGeSet( bUseShortenings, bUseClipping );
		m_VpData.mToken = presenter.GetOrigGeSet().GetToken()->Copy();
	}
	return m_VpData.mGeData;*/

	SViewportData data;
	data.mPresId = presenter.objectId();
	data.mUseShorts = bUseShortenings;
	data.mUseClip = bUseClipping;
	data.mToken = presenter.GetOrigGeSet().GetToken()->Copy();

	bool bAdded = false;
	RCAD::CGeDataCollection2& geSet = mCache.GetItem( data, bAdded );

	if ( bAdded )
		geSet = presenter.GetDispGeSet( bUseShortenings, bUseClipping );

	return geSet;
}

CGeDataAccessor* CGeDataAccessor::
Instance()
{
	if ( s_pSinglGeDataAccessor ==  0 )
	{
		s_pSinglGeDataAccessor = new CGeDataAccessor();
	}
	return s_pSinglGeDataAccessor;
}

void CGeDataAccessor::
Clear()
{
	if ( s_pSinglGeDataAccessor != 0 )
	{
		delete s_pSinglGeDataAccessor;
		s_pSinglGeDataAccessor = 0;
	}
}
