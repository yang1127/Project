#pragma once

#include <iostream>
#include <assert.h>
#include <unordered_map>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

using std::endl;
using std::cout;

const size_t MAX_SIZE = 64 * 1024; 
const size_t NFREE_LIST = MAX_SIZE / 8; //FREE_LIST����8�ֽڼ��
const size_t MAX_PAGES = 129; //����128��Ҫ��ϵͳ����
const size_t PAGE_SHIFT = 12; // 4k

inline void*& NextObj(void* obj)
{
	return *((void**)obj); 
}

class FreeList //����������
{
public:
	void Push(void* obj) // ͷ��
	{
		NextObj(obj) = _freelist; 
		_freelist = obj; 
		++_num;
	}

	void* Pop() // ͷɾ
	{	
		void* obj = _freelist; 
		_freelist = NextObj(obj); 
		--_num;
		return obj;
	}

	void PushRange(void* head, void* tail, size_t num) //��һ����Χ�Ĵ�����
	{
		NextObj(tail) = _freelist; 
		_freelist = head; 
		_num += num;
	}

	size_t PopRange(void*& start, void*& end, size_t num) //ȡһ��spanlist������
	{
		size_t actualNum = 0;
		void* prev = nullptr;
		void* cur = _freelist;
		for (; actualNum < num && cur != nullptr; ++actualNum)
		{
			prev = cur;
			cur = NextObj(cur);
		}

		start = _freelist;
		end = prev;
		_freelist = cur;

		_num -= actualNum;

		return actualNum;
	}

	size_t Num()
	{
		return _num;
	}

	bool Empty() 
	{
		return _freelist == nullptr;
	}

	void Clear() 
	{
		_freelist = nullptr;
		_num = 0;
	}
private:
	void* _freelist = nullptr; 
	size_t _num = 0;
};

class SizeClass //������Ŀ���ִ�С����
{
public:

	// [9, 16] + 7 = [16, 23] -> 16 8 4 2 1 
	static size_t _RoundUp(size_t size, size_t alignment) //����
	{
		return (size + alignment - 1)&(~(alignment - 1));
	}

	// ������[1%��10%]���ҵ�����Ƭ�˷�
	static inline size_t RoundUp(size_t size)
	{
		assert(size <= MAX_SIZE);

		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8192) {
			return _RoundUp(size, 128);
		}
		else if (size <= 65536) {
			return _RoundUp(size, 1024);
		}

		return -1;
	}

	// [9��16] + 7 = [16�� 23]
	static size_t _ListIndex(size_t size, size_t align_shift)
	{
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1; 
	}

	static size_t ListIndex(size_t size)
	{
		assert(size <= MAX_SIZE);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 56 }; 
		if (size <= 128)
		{
			return _ListIndex(size, 3); // 2^3 8�ֽڶ���
		}
		else if (size <= 1024)
		{
			return _ListIndex(size - 128, 4) + group_array[0]; //2^4 16�ֽڶ���
		}
		else if (size <= 8192)
		{
			return _ListIndex(size - 1024, 7) + group_array[1] + group_array[0]; //2^7 128�ֽڶ���
		}
		else if (size <= 65536)
		{
			return _ListIndex(size - 8192, 10) + group_array[2] + group_array[1] + group_array[0]; //2^10 1024�ֽڶ���
		}

		return -1;
	}

	// [2,512]��֮��
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0) 
			return 0;

		int num = MAX_SIZE / size; 
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= 12; 
		if (npage == 0)
			npage = 1;

		return npage;
	}
};

#ifdef _WIN32
typedef unsigned int PAGE_ID;
#else
typedef unsigned long long PAGE_ID;
#endif 

struct Span //�����
{
	PAGE_ID _pageid = 0; // ҳ��
	PAGE_ID _pagesize = 0; // ҳ������

	FreeList _freeList;  // ������������
	size_t _objSize = 0; // ������������С
	int _usecount = 0;   // �ڴ�����ʹ�ü���

	Span* _next = nullptr; //����spanǰ��
	Span* _prev = nullptr;
};

class SpanList //˫������˫�򡢴�ͷ��ѭ��
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin() 
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	void PushFront(Span* newspan) //ͷ��
	{
		Insert(_head->_next, newspan);
	}

	void PopFront() //ͷɾ
	{
		Erase(_head->_next);
	}

	void PushBack(Span* newspan) //β��
	{
		Insert(_head, newspan);
	}

	void PopBack() //βɾ
	{
		Erase(_head->_prev);
	}

	void Insert(Span* pos, Span* newspan) //����
	{
		Span* prev = pos->_prev;

		prev->_next = newspan;
		newspan->_next = pos;
		pos->_prev = newspan;
		newspan->_prev = prev;
	}

	void Erase(Span* pos) //ɾ��
	{
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	bool Empty()
	{
		return Begin() == End();
	}

	void Lock()
	{
		_mtx.lock();
	}

	void Unlock()
	{
		_mtx.unlock();
	}

private:
	Span* _head; 
	std::mutex _mtx;
};

inline static void* SystemAlloc(size_t num_page) //��ϵͳ�����ڴ�
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, num_page*(1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk mmap��
#endif
	if (ptr == nullptr) 
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
#endif
}