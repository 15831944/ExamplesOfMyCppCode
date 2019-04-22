#pragma once
// TODO: Add file description header here
#include "definitions.h"
#include "LinkManager.h"
#include "Element.h"
// TODO: Add necessary includes here

namespace dim {
namespace data{

class CTree 
{		
	public:	  
		CTree();
		Handle GetRootElement() const;
		Status GetStatus() const;
		CElement::ConstPtr GetElement(Handle h) const;
		CElement::Ptr GetElement(Handle h);
		CElement::Ptr Insert( const CElement& e, Handle hParent );
		bool Remove(Handle h);
		int GetSize() const;
		
		bool HasLink(Handle h1, Handle h2) const;
        bool IsParent(Handle hParent, Handle hChild) const;
		int LinksCount(Handle h) const;
		int GetUnlinkedList(THandleList& aHandle);
		bool RemoveLink(Handle h1, Handle h2);
		bool RemoveLinksAt(Handle h);
		void RemoveSubTree(Handle h);
		int GetAllChildren(THandleList& aHandle, Handle hElement) const;
		int GetAllParents(THandleList& aHandle, Handle hElement) const;
		
	private:
		typedef std::vector< CElement > TElements;
		typedef TElements::iterator iterator;
		typedef TElements::const_iterator const_iterator;

		void SetRootElement(Handle h);
		iterator Find( Handle h );
		const_iterator const_Find( Handle h ) const;
		Status m_Status;
		TElements m_aElements;
		CLinkManager m_LinkManager;
		Handle hRootElement;

};

} // ns data
} // ns dim