#include "core/resources.hpp"

using namespace bee;

size_t Resource::m_nextGeneratedID = 0;

void Resources::CleanUp()
{
    for (auto it = m_resources.begin(); it != m_resources.end();)
    {
        if (it->second.use_count() == 1)
        {
            it = m_resources.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
