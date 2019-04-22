#include "stdafx.h"
#include "processors.h"
#include "Viewport.h"
#include "DwgContext.h"
#include "DbAccess.h"
#include "RBCSDIMENSIONING\LockedDimensionsAnalizer.h"

using namespace RbCS::Processors;

Empty::Empty( unsigned uSubjects )
: mTargets( uSubjects )
{
}

bool Empty::Run( SDwgContext& ctx )
{
	bool bRes = false;
	
	ctx.mRegenTargets = mTargets;
	
	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->empty( ctx.mRegenTargets );
		bRes = true;
	}
	return bRes;
}

bool RecalcProjection::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->calcProjectComponents();
		bRes = true;
	}
	return bRes;
}

bool GenerateAxes::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->generateAxes();
		bRes = true;
	}
	return bRes;
}

bool GenerateWelds::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->generateWelds();
		bRes = true;
	}
	return bRes;
}

bool GenerateWorkframe::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->generateWorkframe();
		bRes = true;
	}
	return bRes;
}

bool ResetDwgBuilder::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		const AcDbObjectId subDocId = eVp->GetSubDocId(); 
		const AcDbObjectId builderId = RBCS_DWGBUILDERID( RdSubDoc( subDocId ).object() ); 
		EdDwgBuilder( builderId )->PrepareObjects();
		bRes = true;
	}
	return bRes;
}

bool GenerateDrawing::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->PrepareViewport();
		AcDbObjectId idNewRep = eVp->generatePresentor();
		
		if ( ctx.mClipPresent )
			if ( EdDrawingPresenter ePr = EdDrawingPresenter( idNewRep ) )
			{
				ePr->SetClipping( ctx.mClippingData.first, ctx.mClippingData.second );
			}

		bRes = true;
	}
	return bRes;
}

bool RotateCutDetailMarks::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		double rotation = eVp->GetAngle();
		if ( fabs( rotation ) > algo::kTol.equalVector() )
		{
			eVp->SetAngle( rotation );
			eVp->transformBy( AcGeMatrix3d::rotation( rotation, AcGeVector3d::kZAxis ), false );
			{
				AcGeMatrix3d rtMtrx( AcGeMatrix3d::rotation( rotation, AcGeVector3d::kZAxis ) );
				rtMtrx = rtMtrx.invert();

				AcDbObjectIdArray aMarker;
				eVp->GetCutObjects( aMarker );
				eVp->GetDetailsObjects( aMarker );

				int uObjectsCount = aMarker.length();
				for( int i = 0; i < uObjectsCount; i++ )
				{
					AcDbEntityPointer pMarker( aMarker[i], AcDb::kForWrite );
					if( pMarker.openStatus() == Acad::eOk )
						pMarker->transformBy( rtMtrx );
				}
			}
		}
		bRes = true;
	}
	return bRes;
}

bool MakeDimensioning::Run( SDwgContext& ctx )
{
	bool bRes = false;

	if ( AcDbEditor<RbCSViewport> eVp = AcDbEditor<RbCSViewport>( ctx.mViewportId ) )
	{
		eVp->dimensioning();
		bRes = true;
	}
	return bRes;
}

bool PreAnalizeLockedDim::Run( SDwgContext& ctx )
{
	RbCSGetService< IRbCSLockedDimensionsAnalizer > lockedAnilyzer( eRBCS_SRV_LOCKDIM );
	IRbCSLockedDimensionsAnalizer* serv = lockedAnilyzer;
	if ( serv != NULL )
	{
		ILockedDimensionsAnalizer* lockedAn = serv->GetLockedDimensionsAnalizer();
		if ( lockedAn != NULL )
		{
			lockedAn->SetDimSnapFeature( ctx.mViewportId );
		}
	}
	return true;
}

bool PostAnalizeLockedDim::Run( SDwgContext& ctx )
{
		RbCSGetService< IRbCSLockedDimensionsAnalizer > lockedAnilyzer( eRBCS_SRV_LOCKDIM );
	IRbCSLockedDimensionsAnalizer* serv = lockedAnilyzer;
	if ( serv != NULL )
	{
		ILockedDimensionsAnalizer* lockedAn = serv->GetLockedDimensionsAnalizer();
		if ( lockedAn != NULL )
		{
			lockedAn->DeleteNeedlessDim( ctx.mViewportId );
		}
	}
	return true;
}