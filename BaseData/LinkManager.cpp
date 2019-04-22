//--------------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "LinkManager.h"
using namespace dim::data;
//--------------------------------------------------------------------------------------------------------------

template < Relation::Type type >
void CLinkManager::GetAllRelations( THandleList &aHandle, const Handle h ) const
{
	const_iterator it = m_aLink.begin(), endit = m_aLink.end();

	FnHasRelation<static_cast<Relation::Type>( Relation::Invert<type>::Value )> func( h );
	
	while (it != endit)
	{
		it = std::find_if(it, endit, func);
		if (it != endit)
		{
			aHandle.push_back( CLink::FnGetRelation< type >().Get(*it) );
			it++;
		}
	}	
}

CLinkManager::TLinksList& CLinkManager::GetLinksList() 
{
	return m_aLink;
}


bool CLinkManager::AddLink(CLink& link)
{
	if (HasLink(link))
		return false;
	m_aLink.push_back(link);
	return true;
}
//--------------------------------------------------------------------------------------------------------------
CLinkManager::iterator CLinkManager::Find(Handle h1, Handle h2) 
{
	return std::find_if(m_aLink.begin(), m_aLink.end(), CLink::FnNonrigEqual(CLink(h1, h2)));
}
//--------------------------------------------------------------------------------------------------------------
bool CLinkManager::HasLink(const CLink& link) const
{
	if (std::find_if(m_aLink.begin(), m_aLink.end(), CLink::FnNonrigEqual(link)) == m_aLink.end())
		return false;
	return true;
}
//--------------------------------------------------------------------------------------------------------------
bool CLinkManager::IsParent(Handle hParent, Handle hChild) const
{
	if (std::find_if(m_aLink.begin(), m_aLink.end(), CLink::FnStrongEqual(CLink(hParent, hChild))) == m_aLink.end())
		return false;
	return true;
}
//--------------------------------------------------------------------------------------------------------------
bool CLinkManager::RemoveLink(Handle h1, Handle h2)
{
	iterator it = std::find_if(m_aLink.begin(), m_aLink.end(), CLink::FnNonrigEqual(CLink(h1, h2)));
	if (it != m_aLink.end())
	{
		m_aLink.erase(it);
		return true;
	}
	return false;
}

bool CLinkManager::RemoveLinksAt(Handle h)
{
	bool b=false;
	iterator it = m_aLink.begin();
	while (it != m_aLink.end())
	if ( ( (*it).GetChild()==h ) || ( (*it).GetParent()==h ) )
	{
		b=false;
		it = m_aLink.erase(it);
		b=true;
		if (it == m_aLink.end()) break;
		
	}
	else it++;
	return b;
}

//-----------------------------------------------------------------------------------------------------------------
void CLinkManager::GetAllChildren(THandleList &aHandle, Handle hParent) const
{
	GetAllRelations< Relation::Child > ( aHandle, hParent );
}
//-----------------------------------------------------------------------------------------------------------------
void CLinkManager::GetAllParents(THandleList &aHandle, Handle hChild) const
{
	GetAllRelations< Relation::Parent > ( aHandle, hChild );
}


/*void CLinkManager::GetUnlinkedList(THandleList &aHandle) const
{
	const_iterator it = m_aLink.begin(), endit = m_aLink.end();
	while (it != endit)
	{
		it = std::find_if(it, endit, true);
		if (it != endit)
		{
			aHandle.push_back( *it );
			it++;
		}
	}
}  */

void CLinkManager::GetLinkedHandleList(THandleList& aHfromLinks)
{	
	iterator It  = m_aLink.begin();
	iterator eIt = m_aLink.end(); 
	for (; It != eIt ; ++It)
	{
		aHfromLinks.push_back(It->GetParent());
		aHfromLinks.push_back(It->GetChild());
	};
	std::unique(aHfromLinks.begin(),aHfromLinks.end());
}

int CLinkManager::LinksCount(Handle h) const
{
	int counter=0;
	const_iterator it = m_aLink.begin();
	const_iterator endIt = m_aLink.end();
	for ( ; it != endIt; it++ )
	{
		if ( ( h == it->GetParent() ) || ( h == it->GetChild() ) )
		{
			counter++;
		}	
	}
	return counter;
}

//-----------------------------------------------------------------------------------------------------------------