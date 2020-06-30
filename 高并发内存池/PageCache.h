#pragma once

#include "Common.h"

class PageCache
{
public:
	// ����һ����span
	Span* _NewSpan(size_t numpage);
	Span* NewSpan(size_t numpage);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

	Span* GetIdToSpan(PAGE_ID id);

	static PageCache& GetPageCacheInstance()
	{
		static PageCache inst;
		return inst;
	}

private:
	// ����ģʽ
	PageCache()
	{}

	PageCache(const PageCache&) = delete;

	SpanList _spanLists[MAX_PAGES];
	std::unordered_map<PAGE_ID, Span*>  _idSpanMap;

	std::mutex _mtx;
};
