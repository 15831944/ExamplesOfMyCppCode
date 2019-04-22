#include "stdafx.h"
#include "Element.h"

using namespace dim::data;

CElement::CElement( Handle h ) 
: m_Handle(h)
{
}

Handle CElement::GetHandle() const
{
	return m_Handle;
}
