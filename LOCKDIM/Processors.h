#pragma once
#include "DwgProcessorBase.h"

namespace RbCS
{

namespace Processors
{
using namespace RbCS::Drawings;

class Empty : public RbCS::Drawings::CProcessorBase
{
public:
	Empty( unsigned uSubjects );

private:
	bool Run( SDwgContext& ctx );
	unsigned mTargets;
};

class RecalcProjection : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class GenerateAxes : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};
	
class GenerateWelds : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class GenerateWorkframe : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class ResetDwgBuilder : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class GenerateDrawing : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class RotateCutDetailMarks : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class MakeDimensioning : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class PreAnalizeLockedDim : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

class PostAnalizeLockedDim : public RbCS::Drawings::CProcessorBase
{
private:
	bool Run( SDwgContext& ctx );
};

} // ns Processors
} // ns RbCS