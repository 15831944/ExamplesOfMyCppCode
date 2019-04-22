//----------------------------------------------------------------------------------------------------------------	
#include "stdafx.h"
#include "Link.h"
using namespace dim::data;
//----------------------------------------------------------------------------------------------------------------	
CLink::CLink (Handle hParent, Handle hChild) : m_pair (hParent, hChild) { }
//----------------------------------------------------------------------------------------------------------------	
CLink::FnStrongEqual::FnStrongEqual (const CLink& link) { m_pair = link.m_pair; }
//----------------------------------------------------------------------------------------------------------------	
CLink::FnNonrigEqual::FnNonrigEqual (const CLink& link) { m_pair = link.m_pair; }
//----------------------------------------------------------------------------------------------------------------	
bool CLink::FnStrongEqual::operator()(const CLink& link) const
{
	//HandlePair pair = link.GetPair();
	return (m_pair.first == link.GetParent()) && (m_pair.second == link.GetChild());
}
//----------------------------------------------------------------------------------------------------------------	
bool CLink::FnNonrigEqual::operator()(const CLink& link) const
{
	return (((m_pair.first == link.GetParent() ) && (m_pair.second == link.GetChild())) ||
			((m_pair.first == link.GetChild()) && (m_pair.second == link.GetParent())));
}
//----------------------------------------------------------------------------------------------------------------	
void CLink::SetParent(CElement& parentElem)
{
	m_pair.first=parentElem.GetHandle();
}

void CLink::SetChild(CElement& childElem)
{
	m_pair.second=childElem.GetHandle();
}

Handle CLink::GetParent() const
{
	return m_pair.first;
}

Handle CLink::GetChild() const
{
	return m_pair.second;
}

HandlePair CLink::GetLink() const
{
	return m_pair;
}
//----------------------------------------------------------------------------------------------------------------	