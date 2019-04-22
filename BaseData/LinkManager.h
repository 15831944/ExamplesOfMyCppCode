#pragma once
// TODO: Add file description header here
#include "definitions.h"
#include "Link.h"
// TODO: Add necessary includes here

namespace dim {
namespace data{

class CLinkManager
{
	friend class CTree;

	typedef std::vector<CLink> TLinksList;

	typedef TLinksList::iterator iterator;
	typedef TLinksList::const_iterator const_iterator;
 
	TLinksList& GetLinksList();
	bool AddLink (CLink& link);
	bool HasLink (const CLink& link) const;
	bool IsParent (Handle hParent, Handle hChild) const;
	iterator Find (Handle h1, Handle h2);
	bool RemoveLink ( Handle h1, Handle h2);
	bool RemoveLinksAt (Handle h);
	void GetAllChildren (THandleList& aHandle, Handle hParent) const;
	void GetAllParents (THandleList& aHandle, Handle hChild) const;
	void GetLinkedHandleList(THandleList& aHfromLinks);
	int LinksCount (Handle h) const;
	
	template < Relation::Type Type > struct FnHasRelation;

	template < Relation::Type type >
	void GetAllRelations( THandleList& aHandle, const Handle handle ) const;

	TLinksList m_aLink;
};

template < Relation::Type type >
struct CLinkManager::FnHasRelation
{
	Handle handle;
	FnHasRelation(Handle h) : handle(h) {}
	bool operator()(const CLink& link) const
	{
		return CLink::FnGetRelation< type >().Get(link) == handle;
	}
};



} // ns data
} // ns dim