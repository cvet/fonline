#ifndef __TRACYTIMELINECONTROLLER_HPP__
#define __TRACYTIMELINECONTROLLER_HPP__

#include <assert.h>
#include <vector>

#include "../public/common/TracyForceInline.hpp"
#include "tracy_robin_hood.h"
#include "TracyTimelineItem.hpp"

namespace tracy
{

class TimelineController
{
public:
    TimelineController( View& view, Worker& worker );

    void FirstFrameExpired();
    void Begin();
    void End( double pxns, int offset, const ImVec2& wpos, bool hover, float yMin, float yMax );

    template<class T, class U>
    void AddItem( U* data )
    {
        auto it = m_itemMap.find( data );
        if( it == m_itemMap.end() ) it = m_itemMap.emplace( data, std::make_unique<T>( m_view, m_worker, data ) ).first;
        m_items.emplace_back( it->second.get() );
    }

    float GetHeight() const { return m_height; }
    const unordered_flat_map<const void*, std::unique_ptr<TimelineItem>>& GetItemMap() const { return m_itemMap; }

    tracy_force_inline TimelineItem& GetItem( const void* data )
    {
        auto it = m_itemMap.find( data );
        assert( it != m_itemMap.end() );
        return *it->second;
    }

private:
    std::vector<TimelineItem*> m_items;
    unordered_flat_map<const void*, std::unique_ptr<TimelineItem>> m_itemMap;

    float m_height;
    float m_scroll;

    bool m_firstFrame;

    View& m_view;
    Worker& m_worker;
};

}

#endif
