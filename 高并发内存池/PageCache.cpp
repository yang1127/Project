#include "PageCache.h"

Span* PageCache::_NewSpan(size_t numpage)
{
	//如果这个位置有，则直接在这个位置取
	if (!_spanLists[numpage].Empty())
	{
		Span* span = _spanLists[numpage].Begin();
		_spanLists[numpage].PopFront();
		return span;
	}

	//若这个位置没有，则不需要走这个位置
	for (size_t i = numpage + 1; i < MAX_PAGES; ++i) 
	{
		if (!_spanLists[i].Empty())
		{
			// 分裂
			Span* span = _spanLists[i].Begin();
			_spanLists[i].PopFront();

			Span* splitspan = new Span;
			//尾切
			splitspan->_pageid = span->_pageid + span->_pagesize - numpage;
			splitspan->_pagesize = numpage;
			for (PAGE_ID i = 0; i < numpage; ++i) 
			{
				_idSpanMap[splitspan->_pageid + i] = splitspan; 
			}

			span->_pagesize -= numpage;

			_spanLists[span->_pagesize].PushFront(span);

			return splitspan; 
		}
	}

	//找系统申请
	void* ptr = SystemAlloc(MAX_PAGES - 1);

	Span* bigspan = new Span; 
	bigspan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT; 
	bigspan->_pagesize = MAX_PAGES - 1; 

	for (PAGE_ID i = 0; i < bigspan->_pagesize; ++i) 
	{
		_idSpanMap[bigspan->_pageid + i] = bigspan; 
	}

	_spanLists[bigspan->_pagesize].PushFront(bigspan);

	return _NewSpan(numpage);
}

Span* PageCache::NewSpan(size_t numpage)
{
	_mtx.lock();
	Span* span = _NewSpan(numpage);
	_mtx.unlock();

	return span;
}

void PageCache::ReleaseSpanToPageCache(Span* span) //还给 page cache
{
	// 页合并：先向前、再向后
	// 向前合并
	while (1)
	{
		PAGE_ID prevPageId = span->_pageid - 1;
		auto pit = _idSpanMap.find(prevPageId);
		// 前面的页不存在
		if (pit == _idSpanMap.end())
		{
			break;
		}

		// 说明前一个还在使用中，不能合并
		Span* prevSpan = pit->second; 
		if (prevSpan->_usecount != 0)
		{
			break;
		}

		//如果合并超过128页的span，不要合并
		if (span->_pagesize + prevSpan->_pagesize >= MAX_PAGES)
		{
			break;
		}

		// 合并
		span->_pageid = prevSpan->_pageid;
		span->_pagesize += prevSpan->_pagesize;
		for (PAGE_ID i = 0; i < prevSpan->_pagesize; ++i)
		{
			_idSpanMap[prevSpan->_pageid + i] = span; 
		}

		_spanLists[prevSpan->_pagesize].Erase(prevSpan); 
		delete prevSpan;
	}

	// 向后合并
	while (1)
	{
		PAGE_ID nextPageId = span->_pageid + span->_pagesize;
		auto nextIt = _idSpanMap.find(nextPageId);
		//后面的页不存在
		if (nextIt == _idSpanMap.end())
		{
			break;
		}

		//后面的页，使用中
		Span* nextSpan = nextIt->second;
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		if (span->_pagesize + nextSpan->_pagesize >= MAX_PAGES)
		{
			break;
		}

		//合并
		span->_pagesize += nextSpan->_pagesize;
		for (PAGE_ID i = 0; i < nextSpan->_pagesize; ++i)
		{
			_idSpanMap[nextSpan->_pageid + i] = span;
		}

		_spanLists[nextSpan->_pagesize].Erase(nextSpan); 
		delete nextSpan;
	}

	//将page cache中的span挂入
	_spanLists[span->_pagesize].PushFront(span);
}

Span* PageCache::GetIdToSpan(PAGE_ID id)
{
	auto it = _idSpanMap.find(id);
	if (it != _idSpanMap.end())
	{
		return it->second; 
	}
	else
	{
		return nullptr;
	}
}