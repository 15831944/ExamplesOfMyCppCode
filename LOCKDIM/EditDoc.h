// RbCSEditDoc.h: interface for the RbCSEditDoc class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(AFX_RBCSEDITDOC_H__1522AB43_1781_11D5_9794_0080C879A7C4__INCLUDED_)
#define AFX_RBCSEDITDOC_H__1522AB43_1781_11D5_9794_0080C879A7C4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RbCSDrawing.h"
#include "RbCTDrawingObj\RbctDEditDoc.h"
#include "RbCSHeader\Positioning.h"

#pragma warning(push)
#pragma warning(disable: 4275 4251)

class RbCSSubDoc;
class RbCTDOGroup;
class RbCSDwgBuilder;

class _RBCSDRAWING_CLASS RbCSEditDoc : public RbctDEditDocument  
{
private:
    static Adesk::UInt16 s_uVersion;
    Adesk::UInt16 m_uReadedVersion;

public:
	ACRX_DECLARE_MEMBERS(RbCSEditDoc);

    RbCSEditDoc();
    RbCSEditDoc( RBCSPositioning::eLevel iPosLevel );
    RbCSEditDoc( double Width, double Height, bool bFitToWindow = true);
    virtual ~RbCSEditDoc();

    bool Init( vector<AcDbObjectId>& acadViewports, bool bFromTemplate = false);

    AcDbObjectId GetParentViewportId( const AcRxClass *pViewPortClass );

public:
    //{{AFX_ARX_METHODS(RbCSEditDoc)
    virtual Acad::ErrorStatus dwgInFields( AcDbDwgFiler *pFiler );
    virtual Acad::ErrorStatus dwgOutFields( AcDbDwgFiler *pFiler ) const;

    virtual Acad::ErrorStatus dxfInFields( AcDbDxfFiler *pFiler );
    virtual Acad::ErrorStatus dxfOutFields( AcDbDxfFiler *pFiler ) const;
    
    Acad::ErrorStatus subGetClassID(CLSID* pClsid) const;
    Acad::ErrorStatus subErase( Adesk::Boolean erasing );
    //}}AFX_ARX_METHODS

	//{{Events
    void OnInitEditDocument();
	void PreCreateEditDocument();
	void OnCreateDocument();
	//}}Events

	AcDbObjectId getModelObjectId() const;
	AcDbObjectId getPositionId() const;

    RBCSPositioning::eLevel GetPosLevel() const;
    virtual void SetPosLevel( RBCSPositioning::eLevel iPosLevel );

    void SetDimStyleName( const TCHAR *pcsName );
	void GetDimStyleName( RBCTString &rsName ) const;
	const RBCTString& GetDimStyleName() const { return m_sDimStyleName; }

	void SetDimStyleId( const AcDbObjectId &rDimStyleId );
	AcDbObjectId GetDimStyleId() const;

	static void SetTemplateDimStyleId( const AcDbObjectId &rDimStyleId );
	static AcDbObjectId GetTemplateDimStyleId() { return( s_templateDimStyleId ); }

	void SetDefaultCutName( const TCHAR *pcsName );
	void GetDefaultCutName( RBCTString &rsName ) const;

	void SetDefaultDetailName( const TCHAR *pcsName );
	void GetDefaultDetailName( RBCTString &rsName ) const;

	AcDbObjectId getViewportByMarker( const AcDbObjectId &rMarkerId ) const;

    // implementation of RbctUIItem interface
    virtual bool GetAtomData( RbctUIAtom& AtomData );

protected:
	bool OnRefreshDocument();
	bool m_bRefreshing;

private:

	/* performs initialization of DimSupervisorService */
	void DimSupervisorServiceIni(const AcDbObjectId& subDocId) const;

	void RegenViewportsList( const std::vector< AcDbObjectId >& lst, AcDbObjectId subdocId );

	AcGeMatrix3d GetWorkingOCS( const RbCSDwgBuilder* cpBuilder );

protected:
    RBCSPositioning::eLevel m_iPosLevel;

	RBCTString m_sDefaultCutName;
	RBCTString m_sDefaultDetailName;
    RBCTString m_sDimStyleName;
	
	AcDbObjectId m_dimStyleId;

	static AcDbObjectId s_templateDimStyleId;
};

MAKE_ACDBOPENOBJECT_FUNCTION(RbCSEditDoc);

typedef AcDbObjectPointer<RbCSEditDoc> RbCSEditDocPtr;

#pragma warning(pop)

#endif // !defined(AFX_RBCSEDITDOC_H__1522AB43_1781_11D5_9794_0080C879A7C4__INCLUDED_)
