#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocte(size_t size) //�����ڴ�
{
	size_t index = SizeClass::ListIndex(size);
	FreeList& freeList = _freeLists[index];
	if (!freeList.Empty()) 
	{
		return freeList.Pop(); 
	}
	else 
	{
		return FetchFromCentralCache(SizeClass::RoundUp(size));
	}
}

void ThreadCache::Deallocte(void* ptr, size_t size) //�ͷ��ڴ�
{
	size_t index = SizeClass::ListIndex(size); 
	FreeList& freeList = _freeLists[index]; 
	freeList.Push(ptr);  

	size_t num = SizeClass::NumMoveSize(size);
	if (freeList.Num() >= num) //�ﵽһ������������ CentralCache
	{
		ListTooLong(freeList, num, size); 
	}
}

void ThreadCache::ListTooLong(FreeList& freeList, size_t num, size_t size) //����central cache�е�span
{
	void* start = nullptr, *end = nullptr;
	freeList.PopRange(start, end, num);

	NextObj(end) = nullptr;
	CentralCache::GetInsatnce().ReleaseListToSpans(start, size); 
}

// ����thread cache 
void* ThreadCache::FetchFromCentralCache(size_t size) //��Ҫ����
{
	size_t num = SizeClass::NumMoveSize(size); 

	//CentralCache�л�ȡ�ڴ�
	void* start = nullptr, *end = nullptr;
	size_t actualNum = CentralCache::GetInsatnce().FetchRangeObj(start, end, num, size);

	if (actualNum == 1)
	{
		return start;
	}
	else
	{
		size_t index = SizeClass::ListIndex(size);
		FreeList& list = _freeLists[index];
		list.PushRange(NextObj(start), end, actualNum - 1);

		return start;
	}
}