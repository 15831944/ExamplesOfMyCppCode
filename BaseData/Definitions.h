#pragma once

// TODO: Add file description header here
#include <vector>
#include <utility>
#include <algorithm>

//#include "..\RbCSDimensioning.h"

// TODO: Add necessary includes here

namespace dim {
namespace data{

typedef int Handle;
typedef std::pair<Handle,Handle> HandlePair;
typedef std::vector<Handle> THandleList;
typedef THandleList::iterator HandleListIterator;
 
struct Relation
{
	enum Type 
	{
		Child   =-1, 
		Sibling = 0,
		Parent  = 1 
	};
	
	template < typename Relation::Type Type >
	struct Invert
	{
	};
	
	template <>
	struct Invert< Relation::Child >
	{
		enum Type { Value = Relation::Parent };
	};
	
	template <>
	struct Invert< Relation::Parent >
	{
		enum Type { Value = Relation::Child };
	};
};

enum Status
{
	Ok = 0,
	Fail
};

} // ns data
} // ns dim