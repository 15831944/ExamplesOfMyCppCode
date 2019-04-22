//-----------------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "Tree.h"
//-----------------------------------------------------------------------------------------------------------------
using namespace dim::data;
//-----------------------------------------------------------------------------------------------------------------
CTree::CTree() 
: m_Status(Fail)
{ 
	m_Status = Ok;
}
//-----------------------------------------------------------------------------------------------------------------
Status CTree::GetStatus() const
{
	return m_Status; 
}

//-----------------------------------------------------------------------------------------------------------------

CElement::ConstPtr CTree::GetElement(Handle h) const
{
	CElement::ConstPtr ret=&m_aElements.back();
	if (h!=(ret->GetHandle()))
		ret=&(*const_Find(h));
	return ret;
}

CElement::Ptr CTree::GetElement(Handle h)
{
	CElement::Ptr ret=&m_aElements.back();
	if (h!=(ret->GetHandle()))
		ret=&(*Find(h));
	return ret;
}

Handle CTree::GetRootElement() const
{
	return hRootElement;
}

//-----------------------------------------------------------------------------------------------------------------

void CTree::SetRootElement(Handle h)
{
	hRootElement=h;
}

//-----------------------------------------------------------------------------------------------------------------

CTree::iterator CTree::
Find( Handle h )
{
	iterator it = m_aElements.begin();
	iterator endIt = m_aElements.end();
	for ( ; it != endIt; it++ )
	{
		if ( h == it->GetHandle() )
		{
			break;
		}	
	}
	return it;
} 

CTree::const_iterator CTree::
const_Find( Handle h )   const
{
	const_iterator it = m_aElements.begin();
	const_iterator endIt = m_aElements.end();
	for ( ; it != endIt; it++ )
	{
		if ( h == it->GetHandle() )
		{
			break;
		}	
	}
	return it;
} 

//-----------------------------------------------------------------------------------------------------------------
CElement::Ptr CTree::Insert(const CElement& e, Handle parent)
{
	iterator endIt = m_aElements.end();
	bool bIsRoot = (parent == 0) && m_aElements.empty();
	bool bIsParentExist = Find(parent)!=endIt;
	bool bIsNotSelfInsert = e.GetHandle()!=parent;	
	if ( (bIsRoot || bIsParentExist ) &&  bIsNotSelfInsert)
	{ 
		bool bElementisNotFind = Find(e.GetHandle())==endIt;
		if (bElementisNotFind)
			m_aElements.push_back(e);
			
		CElement::Ptr retVal = &m_aElements.back();
		if (!bIsRoot) 
		{	
			if (!m_LinkManager.AddLink(CLink(parent,e.GetHandle())))
				retVal = 0;
		}
		else SetRootElement(retVal->GetHandle());
		return retVal;	
	}
	return 0;
}
//-----------------------------------------------------------------------------------------------------------------
int CTree::GetSize() const
{
	return m_aElements.size();
}
//-----------------------------------------------------------------------------------------------------------------
bool CTree::HasLink(Handle h1, Handle h2) const
{
	return m_LinkManager.HasLink(CLink(h1, h2));
}
//-----------------------------------------------------------------------------------------------------------------
bool CTree::IsParent(Handle hParent, Handle hChild) const
{
	return m_LinkManager.IsParent(hParent, hChild);
}
//-----------------------------------------------------------------------------------------------------------------
bool CTree::RemoveLink(Handle h1, Handle h2)
{
	return m_LinkManager.RemoveLink(h1,h2);
}

bool CTree::RemoveLinksAt(Handle h)
{
	return m_LinkManager.RemoveLinksAt(h);
}

bool CTree::Remove(Handle h)
{
	iterator it=Find(h);
	if (it==m_aElements.end()) 
		return false;
	else 
	{ 
		RemoveLinksAt(h); 
		m_aElements.erase(it);
		return true; 
	}
	
}

void CTree::RemoveSubTree(Handle h)
{
	THandleList aHandle;
	int numOfChildren=GetAllChildren(aHandle,h);

	if (numOfChildren)  
	{ 
		THandleList::iterator it=aHandle.begin(), endIt=aHandle.end();
		for (;it!=endIt;it++)
			{
				RemoveSubTree(*it); 
			}
	};
	Remove(h);	
	
}


//-----------------------------------------------------------------------------------------------------------------
int CTree::GetAllChildren(THandleList& aHandle, Handle hParent) const
{
	m_LinkManager.GetAllChildren(aHandle, hParent);
	return aHandle.size();
}
//-----------------------------------------------------------------------------------------------------------------
int CTree::GetAllParents (THandleList& aHandle, Handle hChild) const
{
	m_LinkManager.GetAllParents(aHandle, hChild);
	return aHandle.size();
}

/*int CTree::GetUnlinkedList (THandleList& aHandle) 
{
	iterator Eit, bEIt = m_aElements.begin();
	iterator eEIt = m_aElements.end();
	for ( Eit=bEIt; Eit != eEIt; ++Eit )
	{
		aHandle.push_back(Eit->GetHandle());	
	}
		    
	CLinkManager::iterator HLIt = m_LinkManager.GetLinksList().begin(), eHLIt=m_LinkManager.GetLinksList().end(); 
	HandleListIterator bHIt=aHandle.begin();
	for (;HLIt!=eHLIt;++HLIt)
	{
		aHandle.erase( std::remove( bHIt, aHandle.end(), HLIt->GetParent() ) );
		aHandle.erase( std::remove( bHIt, aHandle.end(), HLIt->GetChild() ) );
	}
	return aHandle.size();
}  */

int CTree::GetUnlinkedList (THandleList& aHandle) 
{
	aHandle.clear();
	iterator it1, bIt1 = m_aElements.begin();
	iterator eIt1 = m_aElements.end();
	Handle hRroot = GetRootElement();
	Handle currentHandle;
	for ( it1=bIt1; it1 != eIt1; ++it1 )
	{
		currentHandle=it1->GetHandle();
		if (currentHandle!=hRroot)
			aHandle.push_back(currentHandle);	
	}
		
	THandleList	aHfromLinks;
	m_LinkManager.GetLinkedHandleList(aHfromLinks);
	
	HandleListIterator bIt2 = aHandle.begin(),eIt2 = aHandle.end();
	HandleListIterator bIt3 = aHfromLinks.begin(), eIt3 = aHfromLinks.end();
	std::sort(bIt2,eIt2);
	std::sort(bIt3,eIt3);
	bIt2=std::set_difference(bIt2,eIt2,bIt3,eIt3,bIt2);
	aHandle.erase(bIt2,eIt2);
	return aHandle.size();
}

int CTree::LinksCount(Handle h) const
{
	return m_LinkManager.LinksCount(h);
}
//-----------------------------------------------------------------------------------------------------------------