// Edward _T("AddWord") Yablonsky

#if defined(_MSC_VER) && (_MSC_VER >= 1000)
	#pragma once
#endif

#if !defined(RoboCAD__RbCSDrawing_DrawingPresenter_hpp__INCLUDED)
#define RoboCAD__RbCSDrawing_DrawingPresenter_hpp__INCLUDED

#include "DrawingEntity.h"
#include "GeObject.hpp"
#include <map>
#include <list>
#include <gelnsg3d.h>
#include <gemat3d.h>
#include <vector>
#include "DwgPrimitiveSet.hpp"
#include "DwgClipBound.hpp"
#include "SegsOwner.h"
#include "dwg_hatch.h"
#include "GeDataCollection.h"
#include "MultiCache.h"

#include "GeArcCollection.h"
#include "rbcsroot\RbCSDrilling.h"

#pragma warning(push)
#pragma warning(disable: 4275 4251)

class AcDbObjectId;
class RbCSViewport;

namespace RCAD
{

typedef std::list<AcDbSoftPointerId> TSoftPtrIdList;

typedef std::vector<AcDbSoftPointerId> TSoftPtrIdVector;

typedef std::vector<AcDbObjectId> TObjectIdVector;

typedef std::vector< RbCS::CGeObject > TGeCash;
typedef std::vector< RbCS::CGeObject > TGeSet;

struct LessPnt2d : public std::less< AcGePoint2d >
{
	result_type operator() ( const AcGePoint2d& a, const AcGePoint2d& b )const
	{
		return a.isEqualTo( b, m_tol ) ? false : fabs( a.x - b.x ) <= m_tol.equalPoint() ?
			( a.y < b.y ) : ( a.x < b.x );
	}
	LessPnt2d()
	{
		m_tol.setEqualPoint( 0.001 );
		m_tol.setEqualVector( 0.001 );
	}
	AcGeTol m_tol;
};
typedef std::set< AcGePoint2d, LessPnt2d > SetPnts2d;

class TDwgClipBound;

}

class RbCSDwgShortening;

typedef AcArray<AcGeLineSeg2d, AcArrayObjectCopyReallocator<AcGeLineSeg2d> >
	AcGeLineSeg2dArray;

class
#ifdef _RBCSDRAWING_
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
RbCSDrawingPresenter : public RbCSDrawingEntity, public ISegmentsOwner
{

friend class RbCSDwgShortening;
friend class CGeDataAccessor;
//typedef RCAD::TObjSet<RCAD::TGeObjectBase*> TGraphObjSet;

public:

// test
#ifdef _DEBUG
	static __int64 num_explodes;
	static __int64 time_explodes;

	static __int64 num_osnaps;
	static __int64 time_osnaps;

	static __int64 num_explArcs;
	static __int64 time_explArcs;
#endif
// test

__declspec(property(get=GetOCS)) const AcGeMatrix3d& OCS;
__declspec(property(get=GetViewDir,put=SetViewDir))
		const AcGeVector3d& ViewDir;
__declspec(property(get=GetUpVector,put=SetUpVector))
		const AcGeVector3d& UpVector;
__declspec(property(get=GetHiddenLinesIsVisible,put=SetHiddenLinesIsVisible))
		bool HiddenLinesIsVisible;
__declspec(property(get=GetViewportId)) const AcDbObjectId& ViewportId;
__declspec(property(get=GetLinkedObjectId)) const AcDbObjectId& LinkedObjectId;

	ACRX_DECLARE_MEMBERS(RbCSDrawingPresenter);

	RbCSDrawingPresenter();
	virtual ~RbCSDrawingPresenter();

	Acad::ErrorStatus applyPartialUndo(AcDbDwgFiler* UndoFiler,
			AcRxClass* Class);

	Acad::ErrorStatus dwgInFields(AcDbDwgFiler* Filer);

	Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* Filer) const;

	Acad::ErrorStatus subExplode(AcDbVoidPtrArray& EntitySet) const;

	Acad::ErrorStatus subGetClassID(CLSID* pClsid) const;

	void getEcs(AcGeMatrix3d& RetVal) const;

	Acad::ErrorStatus subGetGeomExtents(AcDbExtents& Extents) const;

	// fix bug 3012
	Acad::ErrorStatus getGeomExtentsWithoutRotation(AcDbExtents& Extents) const;
	// end 3012

	Acad::ErrorStatus subGetOsnapPoints(AcDb::OsnapMode OsnapMode,
			int GsSelectionMark, const AcGePoint3d& PickPoint,
			const AcGePoint3d& LastPoint, const AcGeMatrix3d& ViewXform,
			AcGePoint3dArray& SnapPoints, AcDbIntArray& GeomIds) const;

	Adesk::UInt32 subSetAttributes(AcGiDrawableTraits* drawableTraits);

	Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& XForm);

	void subViewportDraw(AcGiViewportDraw* DrawCntx);

	Adesk::Boolean subWorldDraw(AcGiWorldDraw* DrawCntx);

	void xmitPropagateModify() const;

	bool Create(const AcDbObjectId& ObjectId, const AcDbObjectId& ViewportId);

	virtual void AddLineSeg( const RbCS::CGeObject& LineSeg );

	void AddShortening(const AcDbObjectId& Id);

	void RemoveShortening(const AcDbObjectId& Id);

	void RemoveAllShortenings();

	void UnbindShortenings();

	void UnbindHatch();

	const AcGeVector3d& GetViewDir() const;

	void SetViewDir(const AcGeVector3d& Value);

	const AcGeVector3d& GetUpVector() const;

	void SetUpVector(const AcGeVector3d& Value);

	bool GetHiddenLinesIsVisible() const;

	void SetHiddenLinesIsVisible(bool Value);

	const AcGeMatrix3d& GetOCS() const;

	const AcDbObjectId& GetViewportId() const;

	void SetViewportId(const AcDbObjectId& value);

	const AcDbObjectId& GetLinkedObjectId() const;

	void GetShortenings(std::vector<AcDbObjectId>& Vector) const;

	void ToShortedLocation(AcGePoint2dArray& Points,
			AcArray<bool>& HiddenFlags) const;

	void ToOriginalLocation(AcGePoint2dArray& Points,
			bool InvertDirection = false) const;

	void ToShortedPresentation(const AcGeLineSeg2d& LineSeg,
			AcGeLineSeg2dArray &Segments) const;

	AcDbObjectId GetIdByGSMarker(unsigned long GSMarker) const;

	AcGeLineSeg3d GetSegmentByGSMarker(unsigned long GSMarker) const;

	void AssignShortennings(const AcGeLine2d& Axes,
			const std::vector<double>& Locations,
			std::vector<AcDbObjectId>& ShorteningIds);

	void GetCenterSnaps(AcGePoint3dArray& Snaps) const;

	bool Is3dView() const;

	bool IsClipped() const;
	void SetClipping(const AcGePoint2d& CornerA, const AcGePoint2d& CornerB);
	void GetClipping( AcGePoint2d& CornerA, AcGePoint2d& CornerB ) const;

	AcDbObjectId GetSchemeId() const;

	void SetSchemeId(const AcDbObjectId& Value);

	void GetWeldSeams(RCAD::TObjectIdVector& Ids);

	void SetWeldSeams(const RCAD::TObjectIdVector& Ids);

	const RCAD::CGeDataCollection2& GetOrigGeSet() const;

	RCAD::CGeDataCollection& GetDrillDispGeSet( RCAD::CGeDataCollection& origSet, bool useShortennings, bool useClipping ) const;

	void addHatchLoop(const AcGePoint2dArray& points);

	void updateHatch();
	void updateHatch(const RbCSViewport* viewport);

	const RbCS::DwgHatch& dwgHatch() const;

	typedef std::vector<AcGePoint2dArray> Loops;

	void toShortedPresentation(const AcGePoint2dArray& loop,
			Loops& loops) const;

	Acad::ErrorStatus subErase(Adesk::Boolean erasing);

	void getExtents(AcDbExtents& Extents) const;

	AcDbObjectId hatchId() const;

	void resolveHolesPrimitivesVisibility(
			RCAD::CGeDataCollection2& primitives) const;

	void addUserObject(const AcDbObjectId& id, const AcGePlane& plane);

	void addDwgUserObjEntityId(const AcDbObjectId& entityId);

	Acad::ErrorStatus writeGraphObjSet(AcDbDwgFiler* pFiler) const;
	Acad::ErrorStatus writeGraphObjSet_small_blocks( AcDbDwgFiler* pFiler ) const;

	Acad::ErrorStatus readGraphObjSet(AcDbDwgFiler* pFiler);
	Acad::ErrorStatus readGraphObjSet_small_blocks( AcDbDwgFiler* pFiler );

	Acad::ErrorStatus subClose();

	Acad::ErrorStatus subIntersectWith(const AcDbEntity* pEnt,
			AcDb::Intersect intType, AcGePoint3dArray& points,
			int thisGsMarker = 0, int otherGsMarker = 0) const;

	Acad::ErrorStatus subIntersectWith(const AcDbEntity* pEnt,
			AcDb::Intersect intType, const AcGePlane& projPlane,
			AcGePoint3dArray& points, int thisGsMarker = 0,
			int otherGsMarker = 0) const;

	void getUserObjectsExtents(AcDbExtents& extents) const;
	void ClearCompressorCashes();

	static void GlobalEnableSwapping( bool bEnable = true );
	static bool GetGlobalSwappingState();

	static void GlobalEnableSmoothArcs( bool );
	static bool GetGlobalSmoothArcsState();

	static void GlobalEnableImprove( bool );
	static bool GetGlobalUsingImproveState();

	void EnableSwapping();

	bool IsValid() const;

	// cashing external contour
	void SetExternalContour( const AcGePolyline2d& externalContour );
	AcGePolyline2d GetExternalContour() const;

	void CashGeObjects( const RCAD::TGeSet& geSet, bool bAppend );
	void GetCashedGeObjects( RCAD::TGeCash& outStore, bool bClear );
	bool HasGeCash() const;

	void SetReadingTransform( const AcGeMatrix3d& mx );

	void AddSegToDrillCash( const RbCS::CGeObject& seg );

	void AddCircle( RbCS::CGeArc& circ );

	bool GetDrillingGeometry( AcDbObjectId partId, const RbCSDrilling* pDril,
								RCAD::CGeArcCollection& arcs, RCAD::CGeDataCollection& segs ) const;

	bool GetAllSnapPoints( RCAD::SetPnts2d& setPnts ) const;

	const RCAD::CGeArcCollection& GetOrigGeArcSet() const;

	RCAD::CGeArcCollection& GetDispGeArcSet( RCAD::CGeArcCollection& origSet, bool useShortennings, bool useClipping ) const;

private:

	enum TUndoOpCode {
	uocNop = 0,
	uocCreate,
	uocTransformBy,
	uocSetOCS,
	uocSetViewDir,
	uocSetUpVector,
	uocSetHiddenLinesIsVisible,
	uocAddShortening,
	uocRemoveShortening,
	uocRemoveAllShortenings,
	uocRestoreAllShortenings,
	uocSetSchemeId,
	uocSetWeldSeams
	};

	struct UserObjEntry
	{
		AcDbObjectId myObjectId;
		AcGePlane myPlane;
	};

	void SetValid( bool );

	RCAD::CGeDataCollection GetDrillGeSet( const AcDbObjectId& partId, const RbCSDrilling* drill ) const;
	RCAD::CGeArcCollection GetDrillGeArcSet( const AcDbObjectId& partId, const RbCSDrilling& drill ) const;

	RCAD::CGeDataCollection2 GetDispGeSet( bool useShortennings = true, bool useClipping = true ) const;

	void FindShortedLocations(AcGePoint2dArray& Points,
			AcArray<bool>& HiddenFlags,
			const AcDbObjectId& FirstShorteningId) const;

	Acad::ErrorStatus SaveGraphObjSet(AcDbDwgFiler* Filer) const;
	Acad::ErrorStatus LoadGraphObjSet(AcDbDwgFiler* Filer);

	Acad::ErrorStatus SaveGraphObjSet2(AcDbDwgFiler* Filer) const;
	Acad::ErrorStatus LoadGraphObjSet2(AcDbDwgFiler* Filer);

	Acad::ErrorStatus SaveShortenings(AcDbDwgFiler* Filer) const;
	Acad::ErrorStatus LoadShortenings(AcDbDwgFiler* Filer);

	Acad::ErrorStatus writeShortenings(AcDbDwgFiler* filer) const;
	Acad::ErrorStatus readShortenings(AcDbDwgFiler* filer);

	Acad::ErrorStatus SaveWeldSeams(AcDbDwgFiler* Filer) const;
	Acad::ErrorStatus LoadWeldSeams(AcDbDwgFiler* Filer);

	Acad::ErrorStatus writeUserObjEntries(AcDbDwgFiler* pFiler) const;
	Acad::ErrorStatus readUserObjEntries(AcDbDwgFiler* pFiler);

	void EraseAll();

	void AddShortening(RbCSDwgShortening* Shortening);

	void RemoveShortening(RbCSDwgShortening* Shortening);

	void RestoreAllShortenings(AcDbDwgFiler* Filer);

	void getUserObjViewInfo(const UserObjEntry& entry, AcDbObjectId& blockId,
			AcGeScale3d& scale, AcGeMatrix3d& transform) const;

	void drawUserObjects(AcGiViewportDraw* pDrawCntx);

	void drawUserObject(AcGiViewportDraw* pDrawCntx, const UserObjEntry& entry);

	//void getUserObjectsExtents(AcDbExtents& extents) const;

	void updateDwgUserObjEntities();

	void convertUserObjEntries();

	Acad::ErrorStatus writeDwgUserObjEntIds(AcDbDwgFiler* pFiler) const;

	Acad::ErrorStatus readDwgUserObjEntIds(AcDbDwgFiler* pFiler);

	Acad::ErrorStatus explodeArcs(AcDbVoidPtrArray& EntitySet) const;

	typedef std::vector<UserObjEntry> UserObjEntries;
	typedef std::vector<AcDbObjectId> UserObjEntIds;

	AcDbSoftPointerId m_ObjectId;
	AcDbSoftPointerId m_ViewportId;
	
	mutable RCAD::CGeDataCollection2 m_aGeObjects;
	RCAD::CGeDataCollection2::WriterPtr m_WR;

	AcGeMatrix3d m_OCS;
	AcGeVector3d m_ViewDir;
	AcGeVector3d m_UpVector;
	bool m_HiddenLinesIsVisible;
	RCAD::TSoftPtrIdVector m_Shortenings;
	bool m_IsClipped;
	RCAD::TDwgClipBound m_ClipBound;
	AcDbSoftPointerId m_SchemeId;
	RCAD::TSoftPtrIdVector m_WeldSeams;
	RbCS::DwgHatch myHatch;
	AcDbObjectId myHatchId;
	UserObjEntries myUserObjEntries;
	UserObjEntIds myDwgUserObjEntIds;
	//mutable RCAD::TDwgPrimitiveSet myDisplayedSet;
	mutable AcDbExtents myExtents;
	static Adesk::UInt16 s_uVersion;
	Adesk::UInt16 m_uReadedVersion;

	mutable RCAD::CGeArcCollection m_circs;
	size_t m_minGSArcMarker;
	static AcDbEntity* m_pEntity;

	static bool s_bEnableSwapping;		//	ON/OFF swapping presenter to file
	static bool s_bSmoothArcs;			//	ON/OFF arcs reconstruction (including drillings)
	static bool s_bUseIntImprove;		//  ON/OFF manual improve

	// cashing external contour
	AcGePolyline2d m_externalContour;

	mutable bool m_bNewbie; // true when viewport just created and no snaps used yet (uses for update snap points)

	RCAD::TGeCash m_geCash;

	mutable RCAD::CGeDataCollection m_aDrillSegCash;
	RCAD::CGeDataCollection::WriterPtr m_drCashWR;
	AcGeMatrix3d m_mxRdTransform; // temporary data used for reading of old presenters
	
	void* operator new[](size_t) { return 0; }
	void operator delete[](void*) {}
	void *operator new[](size_t, const TCHAR*, int) { return 0; }
};

class
#ifdef _RBCSDRAWING_
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
CGeDataAccessor
{
public:
	CGeDataAccessor();
	RCAD::CGeDataCollection2 GetDispGeSet( const RbCSDrawingPresenter&, bool bUseShortenings, bool UseMachinings );
	static CGeDataAccessor* Instance();
	static void Clear();

private:
	struct SViewportData
	{
		bool mUseClip;
		bool mUseShorts;
		AcDbObjectId mPresId;
		//RCAD::CGeDataCollection2 mGeData;
		Utils::CToken::Ptr mToken;

		SViewportData()
		{
			mToken = Utils::CToken::CreateNew();
		}

		SViewportData( const SViewportData& r )
		{
			CopyFrom( r );
		}

		bool operator== ( const SViewportData& r )
		{
			return mUseClip == r.mUseClip &&
				mUseShorts == r.mUseShorts  &&
				mPresId == r.mPresId &&
				*mToken.get() == *r.mToken.get();
		}

		SViewportData& operator= ( const SViewportData& r )
		{
			CopyFrom( r );
			return *this;
		}

		void CopyFrom( const SViewportData& r )
		{
			mUseClip = r.mUseClip;
			mUseShorts = r.mUseShorts;
			mPresId = r.mPresId;
			mToken = r.mToken->Copy();
		}
	};

	typedef Utils::CCacheItem< SViewportData, RCAD::CGeDataCollection2 > TCacheItem;
	typedef Utils::CMultiCache< TCacheItem, Utils::SSimpleStoreStrategy< TCacheItem, 8 > > TCache;

	TCache mCache;

	static CGeDataAccessor* s_pSinglGeDataAccessor;
};

inline
const RbCS::DwgHatch& RbCSDrawingPresenter::
dwgHatch() const
{
	return myHatch;
}

#include <dbobjptr.h>

typedef AcDbObjectPointer<RbCSDrawingPresenter> TDrawingPresenterPtr;

#pragma warning(pop)

#endif
