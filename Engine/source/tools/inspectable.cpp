#include "tools/inspectable.hpp"

//#ifdef BEE_INSPECTOR

namespace bee
{
std::vector<IToolbar*> EditorBase<IToolbar>::m_editors;
std::vector<IEntityInspector*> EditorBase<IEntityInspector>::m_editors;
std::vector<IPanel*> EditorBase<IPanel>::m_editors;
std::vector<IStatsBar*> EditorBase<IStatsBar>::m_editors;
}

//#endif