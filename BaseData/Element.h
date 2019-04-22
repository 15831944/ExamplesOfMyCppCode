#pragma once
// TODO: Add file description header here
#include "definitions.h"
// TODO: Add necessary includes here

namespace dim {
namespace data{

class CElement 
{
public:
	//size_t  m_uLevel;
	
	typedef CElement* Ptr;
	typedef const CElement* ConstPtr;
	
	CElement( Handle h );
	Handle GetHandle() const;
	
private:
	Handle m_Handle;
	 
};

} // ns data
} // ns dim