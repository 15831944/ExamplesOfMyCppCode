// RbCSEditDoc.cpp: implementation of the RbCSEditDoc class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "mDefs.h"

#include "RbCTKernel\DataObject.h"
#include "RbCTKernel\MdlRepresentor.h"
#include "RbCTKernel\DOKeeper.h"
#include "RbCTKernel\Group.h"
#include "RbCSRoot\DOConnection.h"
#include "RbCSRoot\Styles.hpp"

#include "EditDoc.h"
#include "SubDoc.h"
#include "CrossSectionViewport.h"
#include "DetailsViewport.h"
#include "SectionDetailsParentViewport.h"
#include "DrawingPresenter.hpp"
#include "GroupDwgGen.h"
#include "DwgBuilder.h"

#include "PositionScheme.h"
#include "RbCTKernel/DOScheme.h"
#include "2dScheme.hpp"

#include "RbCSRoot/DOWorkframe.h"

#include "RbCSRoot/DOWelding.h"
#include "WeldSeamFactory.hpp"

#include "wf_drawing.hpp"

#include "ArbitraryIsometricViewport.h"

#include "RbCSDimensioning/DimensioningSupervisorServ.h"
#include "RbCSDimensioning/DimensioningSupervisor.h"

#include "SurviveHelper.h"

#include "Resource.h"
#include "RbcsHeader\RbcsGuid.h"

#include "DetailMarker.h"
#include "CutDimension.h"

#include "DwgProcessor.h"
#include "Processors.h"

//{{AFX_ARX_MACRO
DEFINE_RDMSO_MEMBERS (RbCSEditDoc, RbctDEditDocument, RBCSEDITDOC )
//}}AFX_ARX_MACRO

Adesk::UInt16 RbCSEditDoc::s_uVersion = 5;
AcDbObjectId RbCSEditDoc::s_templateDimStyleId = AcDbObjectId ::kNull;

#define MSG_NOT_ENOUGH_MEM _T("\nNot enough memory to complete requested operation\n")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RbCSEditDoc::RbCSEditDoc() :
    m_iPosLevel( RBCSPositioning::lSingle ),
    RbctDEditDocument( RBCT::Rbcs ),
	m_sDefaultCutName( _T("A1") ), m_sDefaultDetailName( _T("Detail1") ),
	m_uReadedVersion( 0 )
{
   m_bNeedsPosition = true;
}


RbCSEditDoc::RbCSEditDoc( RBCSPositioning::eLevel iPosLevel ) :
    m_iPosLevel( iPosLevel ), RbctDEditDocument( RBCT::Rbcs ),
	m_sDefaultCutName( _T("A1") ), m_sDefaultDetailName( _T("Detail1") ),
	m_uReadedVersion( 0 )
{
   m_bNeedsPosition = true;
}


RbCSEditDoc::RbCSEditDoc( double Width, double Height, bool bFitToWindow ) :
    m_iPosLevel( RBCSPositioning::lSingle ),
    RbctDEditDocument ( RBCT::Rbcs, Width, Height, bFitToWindow),
	m_sDefaultCutName( _T("A1") ), m_sDefaultDetailName( _T("Detail1") ),
	m_uReadedVersion( 0 )
{
   m_bNeedsPosition = true;
}


Acad::ErrorStatus RbCSEditDoc::subErase( Adesk::Boolean erasing )
{
    assertWriteEnabled();

    Acad::ErrorStatus es = RbctDEditDocument::subErase( erasing );

    if( erasing == Adesk::kTrue )
    {
		/*
        if( !m_sDimStyleName.IsEmpty() )
        {
            RbCSDODimensionStyle ds;
            bool bReaded = ds.ReadFromRegistry( m_iPosLevel, m_sDimStyleName );
            if( bReaded )
            {
                ds.DecRefCount( m_iPosLevel );
                ds.WriteToRegistry( m_iPosLevel );
            }
        }
		*/
    }
    else
    {
		/*
        bool bReaded = false;
        RbCSDODimensionStyle ds;
        if( !m_sDimStyleName.IsEmpty() )
            bReaded = ds.ReadFromRegistry( m_iPosLevel, m_sDimStyleName );

        if( !bReaded )
        {
            m_sDimStyleName = RbCSDODimensionStyle::GetDefault( m_iPosLevel, RbCSDimensionStyle::dstCommon );
            bReaded = ds.ReadFromRegistry( m_iPosLevel, m_sDimStyleName );
            if( !bReaded )
                m_sDimStyleName = _T("");
        }

        if( bReaded )
        {
            ds.IncRefCount( m_iPosLevel );
            ds.WriteToRegistry( m_iPosLevel );
        }
		*/
    }

	return( es );
}

bool RbCSEditDoc::Init( vector<AcDbObjectId>& acadViewports, bool bFromTemplate )
{
	bool bRetcode = RbctDEditDocument::Init( acadViewports, bFromTemplate );
	if( bRetcode )
	{
		assertWriteEnabled();

		AcDbDictionary* pDict = OpenSubdocument ( RbCSSubDoc::desc());
		RXASSERT ( pDict != NULL);
		if( pDict != NULL )
		{
			pDict->close();
			bRetcode = true;
		}

		if( IsTemplate() )
		{
			AcDbObjectId styleId = GetTemplateDimStyleId();
			if( styleId.isValid() )
			{
				SetDimStyleId( styleId );
				SetTemplateDimStyleId( AcDbObjectId::kNull );
			}
		}
	}

	return( bRetcode );
}


Acad::ErrorStatus RbCSEditDoc::subGetClassID(CLSID* pClsid) const
{
    assertReadEnabled();

    RBCTString clsidStr;
    clsidStr.Format( _T( "{%s}" ), _T( GUID_SM_RbCSDOGr ) );
    ::CLSIDFromString( clsidStr, pClsid );

    return Acad::eOk;
}


Acad::ErrorStatus RbCSEditDoc::dwgInFields( AcDbDwgFiler *pFiler )
{
	if( pFiler == NULL )
		return( Acad::eNoInputFiler );

	Acad::ErrorStatus es = pFiler->filerStatus();
	RXASSERT( es == Acad::eOk );

    es = RbctDEditDocument::dwgInFields( pFiler );
	RXASSERT( es == Acad::eOk );
    if( es != Acad::eOk )
        return( es );

	assertWriteEnabled();

    // Object Version - must always be the first item.
    //
    es = pFiler->readUInt16( &m_uReadedVersion );
	RXASSERT( es == Acad::eOk );
    if( m_uReadedVersion > s_uVersion )
        return( Acad::eMakeMeProxy );

	if( m_uReadedVersion < 5 )
	{
		//Serialize array of dimensioning objects
		Adesk::UInt32 uCount = 0;
		es = pFiler->readUInt32( &uCount );
		RXASSERT( es == Acad::eOk );
		AcDbObjectIdArray aDimObject( uCount );
		for( Adesk::UInt32 i = 0; i < uCount; i++ )
		{
			AcDbSoftPointerId objectId;
			es = pFiler->readSoftPointerId( &objectId );
			RXASSERT( es == Acad::eOk );
			aDimObject.append( objectId );
		}

		//Read Id of DataObject
		AcDbObjectId doId;
		es = pFiler->readSoftPointerId( static_cast<AcDbSoftPointerId *>( &doId ) );
		RXASSERT( es == Acad::eOk );
	}

    //Read Positioning level
    es = pFiler->readInt32( reinterpret_cast<Adesk::Int32 *>( &m_iPosLevel ) );
	RXASSERT( es == Acad::eOk );

    //Read reference to dimension style
	if( m_uReadedVersion > 3 )
		es = pFiler->readHardPointerId( (AcDbHardPointerId *)&m_dimStyleId );
	else
		es = m_sDimStyleName.dwgInFields( pFiler );
	RXASSERT( es == Acad::eOk );

	//Read default name of Cut
	if( m_uReadedVersion > 1 )
	{
		es = m_sDefaultCutName.dwgInFields( pFiler );
		RXASSERT( es == Acad::eOk );
	}

	//Read default name of Detail
	if( m_uReadedVersion > 2 )
	{
		es = m_sDefaultDetailName.dwgInFields( pFiler );
		RXASSERT( es == Acad::eOk );
	}

	// little hack
	// because every Steel doc needs position, it is set here in dwgIn
	// m_bNeedsPosition is set in constructor too
	m_bNeedsPosition = true;

	es = pFiler->filerStatus();
	RXASSERT( es == Acad::eOk );
    return( es );
}


Acad::ErrorStatus RbCSEditDoc::dwgOutFields( AcDbDwgFiler *pFiler ) const
{
	if( pFiler == NULL )
		return( Acad::eNoInputFiler );

	Acad::ErrorStatus es = pFiler->filerStatus();
	RXASSERT( es == Acad::eOk );

    es = RbctDEditDocument::dwgOutFields( pFiler );
	RXASSERT( es == Acad::eOk );
    if( es != Acad::eOk )
        return( es );

	assertReadEnabled();

    // Object Version - must always be the first item.
    //
    es = pFiler->writeInt16( s_uVersion );
	RXASSERT( es == Acad::eOk );

    //Write Positioning level
    es = pFiler->writeInt32( static_cast<Adesk::Int32>( m_iPosLevel ) );
	RXASSERT( es == Acad::eOk );

    //Write reference to dimension style
    es = pFiler->writeHardPointerId( m_dimStyleId );
	RXASSERT( es == Acad::eOk );

	//Write default name of Cut
	es = m_sDefaultCutName.dwgOutFields( pFiler );
	RXASSERT( es == Acad::eOk );

	//Write default name of Detail
	m_sDefaultDetailName.dwgOutFields( pFiler );
	RXASSERT( es == Acad::eOk );

	es = pFiler->filerStatus();
	RXASSERT( es == Acad::eOk );
    return( es );
}


Acad::ErrorStatus RbCSEditDoc::dxfInFields( AcDbDxfFiler *pFiler )
{
   return( Acad::eOk );
}


Acad::ErrorStatus RbCSEditDoc::dxfOutFields( AcDbDxfFiler *pFiler ) const
{
   return( Acad::eOk );
}


bool RbCSEditDoc::GetAtomData ( RbctUIAtom& AtomData )
{
   bool bRetcode = RbctDEditDocument::GetAtomData ( AtomData);
   AtomData.m_ServiceId = UIService::RcEditDocService;
   return( bRetcode );
}
//=================================================================================
RbCSEditDoc::~RbCSEditDoc()
{
	// fixed -> YD RCAD (Add Template) crashes when this code is not commented
	AcRxClass* pClassDesc = RbCSSubDoc::desc();
	RBCTString name( pClassDesc->name() );
	if ( has( name ) )
	{
		RbCSSubDoc* pSubDoc = RbCSSubDoc::cast( OpenSubdocument( pClassDesc ) );
		if ( pSubDoc != NULL )
		{
			AcDbObjectId subdocId = pSubDoc->objectId();

			if ( acrxServiceIsRegistered( _T("DimSupervisorService") ) )
			{
				RbCSDimSupervisorService* pDimSupervisorService =
					dynamic_cast< RbCSDimSupervisorService* >(
					acrxServiceDictionary->at( _T("DimSupervisorService") ) );

				if ( pDimSupervisorService != NULL )
				{
					IDrillingsPolicy* pDrillingPolicy =
						pDimSupervisorService->GetDimensioningSupervisor()->GetDrillingPolicy();
					pDrillingPolicy->ClearSubDocInfo( subdocId );

					IPartsPolicy* pPartsPolicy =
						pDimSupervisorService->GetDimensioningSupervisor()->GetPartsPolicy();
					pPartsPolicy->ClearSubDocInfo( subdocId );
				}
			}
		}
	}
}

//============================================================================================================================

AcGeMatrix3d RbCSEditDoc::
GetWorkingOCS( const RbCSDwgBuilder* cpBuilder )
{
	AcGeMatrix3d mx = AcGeMatrix3d::kIdentity;
	AcDbObjectId DO_id = cpBuilder->GetPositionDOId();
	const RbCSDwgBuilder::TDOEntry* pDoEntry = cpBuilder->GetDOEntry( DO_id );
	if ( pDoEntry )
	{
		mx = pDoEntry->m_MdlOCS;
	}
	return mx;
}

void
ClearAllPaperMarkers( RbCSViewport* pVP )
{
	int i, size;
	bool bUpVp = false;
	AcDbObjectIdArray aObject;
	if ( pVP->isWriteEnabled() == false )
	{
		pVP->upgradeOpen();
		bUpVp = true;
	}

	try
	{
		pVP->GetDetailsObjects( aObject );
		size = aObject.length();
		for ( i = 0; i < size; ++i )
		{
			AcDbEditor< RbCSDetailMarker > eMarker( aObject[i] );
			if ( eMarker )
				eMarker->erase();
		}

		pVP->GetCutObjects( aObject );
		size = aObject.length();
		for ( i = 0; i < size; ++i )
		{
			AcDbEditor< RbCSCutDimension > eMarker( aObject[i] );
			if ( eMarker )
				eMarker->erase();
		}

		pVP->GetUsrDimObjects( aObject );
		size = aObject.length();
		for ( i = 0; i < size; ++i )
		{
			AcDbEditor< AcDbDimension > eMarker( aObject[i] );
			if ( eMarker )
				eMarker->erase();
		}

		aObject.removeAll();
		pVP->SetCutObjects( aObject );
		pVP->SetDetailsObjects( aObject );
		pVP->SetUsrDimObjects( aObject );
	}
	catch (...) 
	{
	}
	if ( bUpVp )
		pVP->downgradeOpen();
}

//============================================================================================================================
void RbCSEditDoc::OnInitEditDocument()
{
	RbCSSubDoc* pSubDoc = RbCSSubDoc::cast( OpenSubdocument( RbCSSubDoc::desc() ) );
	RXASSERT( pSubDoc != NULL );
	AcDbObjectId subdocId = pSubDoc->objectId();
	bool bClearMarks = false;

	DimSupervisorServiceIni( subdocId );
	{
		AcDbObjectId dwgBuilderId = RBCS_DWGBUILDERID(pSubDoc);
		const AcDbObjectId posDictId = pSubDoc->GetPosDictId();

		bool subDocWasClosed = RbCTRootUtils::ReallyClose( pSubDoc );
		RXASSERT(subDocWasClosed);

		RbCSDwgBuilderPtr pBuilder(dwgBuilderId, AcDb::kForWrite);
		
		// [ KSA: TT2998 - after adding element to group - builder must be updated ]
		// if( pBuilder.openStatus() == Acad::eOk && pBuilder->IsEmpty() )
		{			
			const AcDbObjectId posId = getPositionId();
			const AcDbObjectId docId = objectId();

			pBuilder->Setup( posId, docId, posDictId );
			
			AcGeMatrix3d mxBefore, mxAfter;
			mxBefore = GetWorkingOCS( pBuilder );
			pBuilder->PrepareObjects();
			mxAfter = GetWorkingOCS( pBuilder );
			bClearMarks = ( mxBefore != mxAfter );
		}
	}

	//===================================================================================
    RbCSGetService<RbCSDimensioningServiceI> pDimService( eRBCS_SRV_DIM ); //Get Dimensioning service
    RbCSDimensioningServiceI *pDimServiceI = pDimService;

	UnLockLayerScope LayerScope( GetDocLayerId() ); //Unlock layers belonged to the document

	std::vector< AcDbObjectId > lstMain, lstDet;

	/* Creating representors in each viewport */
	
	// Detail viewports must be generated only after 
	// main viewports generation completed!

	RcIDbObjIterator VPIterator( GetViewportIterator() );
	for( VPIterator.First();  !VPIterator.IsEnd(); ++VPIterator )
	{
		RbCSViewport* pViewport = RbCSViewport::cast( (*VPIterator) );
		if ( RbCSDetailsViewport::cast( pViewport ) )
			lstDet.push_back( pViewport->objectId() );
		else
		{
			lstMain.push_back( pViewport->objectId() );
			if ( bClearMarks ) 
				ClearAllPaperMarkers( pViewport );
		}
	}
	
	RegenViewportsList( lstMain, subdocId );
	if ( bClearMarks == false )
	RegenViewportsList( lstDet, subdocId );

	pSubDoc->close();
}

void RbCSEditDoc::
RegenViewportsList( const std::vector< AcDbObjectId >& lst, AcDbObjectId subdocId )
{
	using namespace RbCS::Processors;
	using namespace RbCS::Drawings;

	RbCSViewport* pViewport = 0, *pVpCrash = 0; // viewport that causes crash

	try
	{
		std::vector< AcDbObjectId >::const_iterator iB, iE;
		iB = lst.begin();
		iE = lst.end();
		for ( ; iB != iE; ++iB )
		{
			RbCTRootUtils::DowngradeOpen _do( this );
			{
				AcDbObjectPointer< RbCSViewport > ptrViewport( *iB, AcDb::kForRead );
				pViewport = ptrViewport.object();

				//acutPrintf( _T("V: --- %s\n"), pViewport->GetUIName());

				std::basic_string< TCHAR > strName;
				strName = ptrViewport->isA()->name();
				CMemoryMeterTool mmt( strName + _T( "::Regen memory used" ) );

				if( pViewport == NULL || pViewport->ShareLayersWithParent() ||
					pViewport->isKindOf( RbCSSectionDetailsParentViewport::desc() ) )
					continue;

				if ( const RbCSCrossSectionViewport* pCsVp = RbCSCrossSectionViewport::cast( pViewport ) )
				{
					if ( pCsVp->GetRelatedViewportId().isValid() == false )
						continue;
				}

				{
					RbCTRootUtils::UpgradeOpen vuo( pViewport );
					pViewport->SetSubDocId( subdocId );
				}

				//if ( pViewport->isWriteEnabled() == false ) {
				//	Acad::ErrorStatus e = pViewport->upgradeOpen();
				//	assert( e == Acad::eOk );
				//}
			}
			pVpCrash = pViewport;

			if ( !pViewport->IsFrozen() )
			{
			CInViewportProcessor( pViewport->objectId() ) 
				<< Empty( RbCSViewport::taAllNotMarkers ) 
				<< RecalcProjection()
				<< ResetDwgBuilder()
				<< GenerateDrawing()
				<< GenerateAxes()
				<< GenerateWelds()
				<< GenerateWorkframe()
				<< RotateCutDetailMarks()
				<< MakeDimensioning()
				<< cmdRun;
			}
			else
			{
			CInViewportProcessor( pViewport->objectId() )
				<< PreAnalizeLockedDim()
				<< Empty( RbCSViewport::taPresenter ) 
				<< RecalcProjection()
				<< ResetDwgBuilder()
				<< GenerateDrawing()
				<< PostAnalizeLockedDim()
			//	<< GenerateAxes()
			//	<< GenerateWelds()
			//	<< GenerateWorkframe()
			//	<< RotateCutDetailMarks()
			//	<< MakeDimensioning()
				<< cmdRun;
			}

			//pViewport->regenerate( RbCSViewport::taAllNotMarkers );
			//pViewport->dimensioning();

			//AcDbReader< RbCSDrawingPresenter > pres( pViewport->GetRepresentorId() );
			//if ( pres.isOpened() )
			//	pres->EnableSwapping();

			pVpCrash = 0;
		} // for 
	}
	catch ( std::bad_alloc e )
	{
		// generation failed because not enough memory
		globalSurviveHelper()->DropBallast();

		if ( pVpCrash )
		{
			RbCTRootUtils::UpgradeOpen vuo( pVpCrash );
			pVpCrash->empty( RbCSViewport::taAll );
		}

		acutPrintf( MSG_NOT_ENOUGH_MEM );

		throw;
	}
}

bool RbCSEditDoc::OnRefreshDocument()
{
	//unsigned long hdTickCount = GetTickCount();

	bool bRes = true;

    //Erase all the DataObjects
    RbCSSubDoc *pSubDoc = RbCSSubDoc::cast( OpenSubdocument( RbCSSubDoc::desc() ) );
    if( pSubDoc == NULL )
        return( false );

	m_bRefreshing = true;
	RbCSViewport* pVpCrash = 0; /* viewport that causes crash */

	try 
	{
		AcDbDictionary *pKeeperDict = NULL;
		Acad::ErrorStatus es =
			pSubDoc->getAt( RBCT_DOKEEPERS_DICTIONARY, (AcDbObject*&)pKeeperDict, AcDb::kForWrite );
		if( es == Acad::eOk )
		{
			#ifdef _ACAD2007 
			AcDbDictionaryIterator *pIter = pKeeperDict->newIterator( );
			#else
			AcDbDictionaryIterator *pIter = pKeeperDict->newIterator( AcRx::kDictCollated );
			#endif
	
			if( pIter != NULL )
			{
				for( ; !pIter->done(); pIter->next() )
				{
					RbCTDOKeeperPtr pKeeper( pIter->objectId(), AcDb::kForWrite );
					if( pKeeper.openStatus() != Acad::eOk )
						continue;

					unsigned uCount = pKeeper->getObjectsCount();
					for( unsigned i = 0; i < uCount; i++ )
					{
						AcDbObjectId doId = pKeeper->getObjectId( i );
						RbCTDataObjectPtr pDO( doId, AcDb::kForWrite );
						if( pDO.openStatus() == Acad::eOk )
							pDO->erase();
					}
				}

				delete pIter;
			}		

			{
				AcDbObjectId builderId = RBCS_DWGBUILDERID( pSubDoc );
				RbCSDwgBuilderPtr pBuilder( builderId, AcDb::kForWrite );
				if( pBuilder.openStatus() == Acad::eOk )
					pBuilder->Empty();
			}

			//Remove all the Positions
			AcDbDictionary *pPosDict = NULL;
			AcDbObjectId posDictId;
			es = pSubDoc->getAt( RBCS_NAME_POSITION, (AcDbObject *&)pPosDict, AcDb::kForWrite );
			if( es == Acad::eOk )
			{
				#ifdef _ACAD2007 
				AcDbDictionaryIterator *pIter = pKeeperDict->newIterator( );
				#else
				AcDbDictionaryIterator *pIter = pKeeperDict->newIterator( AcRx::kDictCollated );
				#endif
			
				if( pIter != NULL )
				{
					for( ; !pIter->done(); pIter->next() )
					{
						AcDbObjectPointer<AcDbObject> pObject( pIter->objectId(), AcDb::kForWrite );
						if( pObject.openStatus() == Acad::eOk )
							es = pObject->erase();
					}

					delete pIter;
				}

				pPosDict->close();
			}

			es = pKeeperDict->erase();
			pKeeperDict->close();
		}

		pSubDoc->close();

		//Regenerate the document
		OnInitEditDocument();
	}
	catch ( std::bad_alloc e )
	{
		bRes = false;
		globalSurviveHelper()->DropBallast();

		if ( pVpCrash ) 
		{
			RbCTRootUtils::UpgradeOpen vuo( pVpCrash );
			pVpCrash->empty( RbCSViewport::taAll );
		}

		acutPrintf( MSG_NOT_ENOUGH_MEM );
	}

	m_bRefreshing = false;

	//acutPrintf( _T("\tUpdate done in %.1f sec\n"), double( GetTickCount() - hdTickCount ) / 1000. );

    return bRes;
}

void SetTransformation(AcGeMatrix3d& Transformation,
		RbCTDwgRepresentor::TViewType ViewType)
{
	switch (ViewType)
	{
		case RbCTDwgRepresentor::vtFront:
			Transformation = RbCTDwgRepresentor::FrontView;
			break;
		case RbCTDwgRepresentor::vtBack:
			Transformation = RbCTDwgRepresentor::BackView;
			break;
		case RbCTDwgRepresentor::vtTop:
			Transformation = RbCTDwgRepresentor::TopView;
			break;
		case RbCTDwgRepresentor::vtBottom:
			Transformation = RbCTDwgRepresentor::BottomView;
			break;
		case RbCTDwgRepresentor::vtLeft:
			Transformation = RbCTDwgRepresentor::LeftView;
			break;
		case RbCTDwgRepresentor::vtRight:
			Transformation = RbCTDwgRepresentor::RightView;
			break;
		case RbCTDwgRepresentor::vtIsometric:
			Transformation = RbCTDwgRepresentor::IsometricView;
			break;
		case RbCTDwgRepresentor::vtDimetric:
			Transformation = RbCTDwgRepresentor::DimetricView;
			break;
	}
}

AcDbObjectId RbCSEditDoc::GetParentViewportId( const AcRxClass *pViewPortClass )
{
    AcDbObjectId  parentVportId;

    RcIIDIterator* pIdIterator = GetViewportIterator(); 
    RcIDbObjIterator* pObjIterator = new RcIDbObjIterator(pIdIterator);
    RcICastIterator<RbctDViewport*,RbctDViewport*,AcDbObject*>* pCastIterator = new     
    RcICastIterator<RbctDViewport*,RbctDViewport*,AcDbObject*> (pObjIterator);

    for ( pCastIterator->First(); !pCastIterator->IsEnd()  && parentVportId.isNull(); ++(*pCastIterator))
    {
        RbctDViewport* pViewport = *(*pCastIterator);
        if( pViewport->IsOriginal() && pViewport->isA()->isDerivedFrom( pViewPortClass ) )
            parentVportId = pViewport->objectId() ;
    }  

    delete pCastIterator;

    return( parentVportId );
}

RBCSPositioning::eLevel RbCSEditDoc::GetPosLevel() const
{
    assertReadEnabled();

    return( m_iPosLevel );
}

void RbCSEditDoc::SetPosLevel( RBCSPositioning::eLevel iPosLevel )
{
    assertWriteEnabled();

    m_iPosLevel = iPosLevel;
}

void RbCSEditDoc::GetDimStyleName(RBCTString &rsName) const
{
    assertReadEnabled();

    rsName = m_sDimStyleName;
}

void RbCSEditDoc::SetDimStyleName(const TCHAR *pcsName)
{
    RBCTString sName( pcsName );
    if( sName.IsEmpty() || sName == m_sDimStyleName )
        return;

	/*
    RbCSDODimensionStyle ds( pcsName );
    if( !ds.ReadFromRegistry( m_iPosLevel ) )
		return;

	RbCSSubDoc *pSubDoc = RbCSSubDoc::cast( OpenSubdocument( RbCSSubDoc::desc() ) );
	if( pSubDoc == NULL )
		return;

    RcIDbObjIterator pVPIterator( GetViewportIterator() );
	for( pVPIterator.First(); !pVPIterator.IsEnd(); ++pVPIterator )
    {
	    //Apply new Style to each viewport
        RbCSViewport* const pViewport = (RbCSViewport *)(*pVPIterator);
		const RBCTString& sDimStyleName = pViewport->GetDimStyleName();
		RBCTDrawing::eViewType iViewType = pViewport->GetViewType();

        if( sDimStyleName.IsEmpty() )
        {
            ds.ResetSysName();
			CopyDimStyleToSubDoc( ds, iViewType, pSubDoc );

            Acad::ErrorStatus es = pViewport->upgradeOpen();
            RXASSERT( es == Acad::eOk );
			if( es == Acad::eOk )
            {
	            const RBCTString& sDimStyleName = ds.GetSysName();
                pViewport->SetDimStyleName( sDimStyleName );
                pViewport->downgradeOpen();
            }
        }
        else
		{
			//Remove old viewport's style
			EraseDimStyleFromSubDoc( sDimStyleName, m_iPosLevel, iViewType, pSubDoc );

			ds.SetSysName( sDimStyleName );
			ds.WriteToRegistry( m_iPosLevel, pSubDoc );
		}
    }

	//Clean up
	pSubDoc->close();

	//Increment references for new style
	ds.SetSysName( pcsName );
	if( ds.ReadFromRegistry( m_iPosLevel ) )
		ds.IncRefCount( m_iPosLevel );

	//Decrement references for old style
	ds.SetSysName( m_sDimStyleName );
    if( ds.ReadFromRegistry( m_iPosLevel ) )
		ds.DecRefCount( m_iPosLevel );

	//Just store new name
	assertWriteEnabled();
	m_sDimStyleName = pcsName;
	*/
}

void RbCSEditDoc::SetDefaultCutName( const TCHAR *pcsName )
{
	RBCTString sName( pcsName );
	if( sName != m_sDefaultCutName )
	{
		assertWriteEnabled();

		m_sDefaultCutName = sName;
	}
}

void RbCSEditDoc::GetDefaultCutName( RBCTString &rsName ) const
{
	assertReadEnabled();

	rsName = m_sDefaultCutName;
}

void RbCSEditDoc::SetDefaultDetailName( const TCHAR *pcsName )
{
	RBCTString sName( pcsName );
	if( sName != m_sDefaultDetailName )
	{
		assertWriteEnabled();
		m_sDefaultDetailName = sName;
	}
}

void RbCSEditDoc::GetDefaultDetailName( RBCTString &rsName ) const
{
	assertReadEnabled();
	rsName = m_sDefaultDetailName;
}

void RbCSEditDoc::SetDimStyleId( const AcDbObjectId &rDimStyleId )
{
	if( m_dimStyleId != rDimStyleId && rDimStyleId.isValid() )
	{
		assertWriteEnabled();

		RbCSDimStylePtr pStyle( rDimStyleId, AcDb::kForRead );
		if( pStyle.openStatus() == Acad::eOk )
		{
			RcIDbObjIterator pVPIterator( GetViewportIterator() );
			for( pVPIterator.First(); !pVPIterator.IsEnd(); ++pVPIterator )
			{
				//Apply new Style to each viewport
				RbCSViewport* const pViewport = (RbCSViewport *)(*pVPIterator);
				AcDbObjectId styleId = pViewport->GetDimStyleId();
				if( IsTemplate() )
				{
					Acad::ErrorStatus es = pViewport->upgradeOpen();
					if( es == Acad::eOk )
					{
						pViewport->SetDimStyleId( rDimStyleId );
						es = pViewport->downgradeOpen();
					}
				}
				else
				{
					RBCTDrawing::eViewType iViewType = pViewport->GetViewType();
					RbCSDimStylePtr pViewportStyle( styleId, AcDb::kForWrite );
					if( pViewportStyle.openStatus() == Acad::eOk )
						pViewportStyle->copyFrom( pStyle.object() );
				}
			}
		}

		m_dimStyleId = rDimStyleId;
	}
}

AcDbObjectId RbCSEditDoc::GetDimStyleId() const
{
	assertReadEnabled();
	return( m_dimStyleId );
}

void RbCSEditDoc::SetTemplateDimStyleId( const AcDbObjectId &rDimStyleId )
{
	if( s_templateDimStyleId != rDimStyleId )
		s_templateDimStyleId = rDimStyleId;
}

void RbCSEditDoc::PreCreateEditDocument()
{
	RbctDEditDocument::PreCreateEditDocument();
}

void RbCSEditDoc::OnCreateDocument()
{
	RbctDEditDocument::OnCreateDocument();
	
	RcIDbObjIterator VPIterator( GetViewportIterator() );
	for( VPIterator.First();  !VPIterator.IsEnd(); ++VPIterator )
	{
		RbCSViewport* pViewport = RbCSViewport::cast( (*VPIterator) );
		//
		RbCSArbitraryIsometricViewport* pArbitraryVieport = 
			RbCSArbitraryIsometricViewport::cast( pViewport );
		if ( pArbitraryVieport != NULL )
		{
			RbCTRootUtils::UpgradeOpen vuo( pArbitraryVieport );

			pArbitraryVieport->regenerate( RbCSViewport::taRecalcProjection );
		} 
	}
}


AcDbObjectId RbCSEditDoc::getModelObjectId() const
{
	assertReadEnabled();

    RbCSPositionPtr pPosition( getPositionId(), AcDb::kForRead );
    if( pPosition.openStatus() != Acad::eOk )
        return( AcDbObjectId::kNull );

	//Does Position have at least one object assigned
	AcDbObjectId doId;
    RcIDbObjIterator DOIterator( pPosition->GetElementIterator() );
    DOIterator.First();
    if( !DOIterator.IsEnd() )
	{
		RbCTDataObject *pDO = RbCTDataObject::cast( *DOIterator );
		if( pDO != NULL )
			doId = pDO->objectId();
	}

	return( doId );
}

AcDbObjectId RbCSEditDoc::getPositionId() const
{
	RbCSEditDoc *_this = const_cast<RbCSEditDoc *>( this );
    return( _this->m_PositionId.GetId() );
}

AcDbObjectId RbCSEditDoc::getViewportByMarker( const AcDbObjectId &rMarkerId ) const
{
	assertReadEnabled();

	AcDbObjectId viewportId;

	RbCSEditDoc *_this = const_cast<RbCSEditDoc *>( this );

	//std::auto_ptr<RcIIDIterator> pVPIter( _this->GetViewportIterator() ); 
	//RcIDbObjIterator VPIterator( pVPIter.get() );
	RcIDbObjIterator VPIterator( _this->GetViewportIterator() );
	for( VPIterator.First();  !VPIterator.IsEnd(); ++VPIterator )
	{
		RbCSViewport* pViewport = static_cast<RbCSViewport *>(*VPIterator);
		if( pViewport == NULL || pViewport->ShareLayersWithParent() )
			continue;
		
		AcDbObjectId dimId;
		RbCSCrossSectionViewport *pSVP = RbCSCrossSectionViewport::cast( pViewport );
		RbCSDetailsViewport *pDVP = RbCSDetailsViewport::cast( pViewport );
		if( pSVP != NULL )
			dimId = pSVP->GetCutDimId();
		if( pDVP != NULL )
			dimId = pDVP->GetDetailMarkerId();
		if( dimId == rMarkerId )
		{
			viewportId = pViewport->objectId();
			break;
		}
	}

	return( viewportId );
}

void RbCSEditDoc::DimSupervisorServiceIni(const AcDbObjectId& subDocId) const
{
	if ( acrxServiceIsRegistered( _T("DimSupervisorService") ) )
	{
		RbCSDimSupervisorService* pDimSupervisorService =
			dynamic_cast< RbCSDimSupervisorService* >(
			acrxServiceDictionary->at( _T("DimSupervisorService") ) );

		if ( pDimSupervisorService != NULL )
		{
			IDrillingsPolicy* pDrillingPolicy =
				pDimSupervisorService->GetDimensioningSupervisor()->GetDrillingPolicy();
			pDrillingPolicy->ClearSubDocInfo( subDocId );

			IPartsPolicy* pPartsPolicy =
				pDimSupervisorService->GetDimensioningSupervisor()->GetPartsPolicy();
			pPartsPolicy->ClearSubDocInfo( subDocId );
		}
	}
}
