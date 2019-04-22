//--------------------------------------------------------------------------------------------------------------
#pragma once
#include "definitions.h"
#include "Element.h"
//--------------------------------------------------------------------------------------------------------------
namespace dim {
namespace data{
//--------------------------------------------------------------------------------------------------------------
class CLink
{	
	HandlePair m_pair;
	
public:	
    CLink (Handle hParent, Handle hChild);
	void SetParent(CElement& parentElem);
	void SetChild(CElement& childElem);

	Handle GetParent() const;
	Handle GetChild() const;
	HandlePair GetLink() const;
	
	struct FnStrongEqual;
	struct FnNonrigEqual;
		
	template < Relation::Type type >
	struct FnGetRelation;
	
	template <>	struct FnGetRelation< Relation::Child >;
	template <>	struct FnGetRelation< Relation::Parent >;	
};


template <>	struct CLink::
FnGetRelation< Relation::Child > 
{
	Handle Get( const CLink& lnk ) const 
	{
		return lnk.GetChild();
	}
};

template <>	struct CLink::
FnGetRelation< Relation::Parent > 
{
	Handle Get( const CLink& lnk ) const 
	{
		return lnk.GetParent();
	}
};


struct CLink::FnStrongEqual
{
	HandlePair m_pair;
	FnStrongEqual (const CLink& link);
	bool operator()(const CLink& link) const;
};

struct CLink::FnNonrigEqual
{
	HandlePair m_pair;
	FnNonrigEqual (const CLink& link);
	bool operator()(const CLink& link) const;
};

//--------------------------------------------------------------------------------------------------------------
} // ns data
} // ns dim
//--------------------------------------------------------------------------------------------------------------
