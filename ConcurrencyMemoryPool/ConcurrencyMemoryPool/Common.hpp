#pragma once

/*
	������������ռ�֮��ʹ��һ���������������ռ� --- ��������
	ȡ�ռ��ʱ��ֱ�������������н���ͷɾ
	�ͷſռ��ʱ��ֱ��������������ͷ��
	��������ռ��ʱ��Ҫ����Ĵ�С�ǲ�һ���ģ���Ϳ��Ը�����������
	�������ø�1��n����������ֻ��Ҫ��2k��4k��8k��64k��128k һֱ��256k���ɡ�
	�����϶�����ڿռ��˷ѵġ�����˵����7�ֽڣ��ڴ�ػ����8�ֽڣ���ֵ1�ֽھ�������Ƭ��
	����Ƭ�������㹻�Ŀռ䣬������Щ�ռ䲻���������ܽ������롣
*/

#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <assert.h>

#include "ObjectPool.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif // _WIN32

#include <cassert>

// ֻ�ܷ���MAX_BYTES���µĿռ�
static const size_t MAX_BYTES = 256 * 1024; 
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGES_SHIFT = 13;

static void*& NextObj(void* obj) {
	return *(void**)obj;
}

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		std::cout << "std::bad_alloc" << std::endl;
		//throw std::bad_alloc();

	return ptr;
}


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}

class FreeList {
public:
	//FreeList()
	//	:_freelist(nullptr),_MaxSize(1)
	//{
	//}

	// ��������
	void PushRange(void* left, void* right, size_t num) {
		NextObj(right) = _freelist;
		_freelist = left;
		_size += num;
	}

	void PopRange(void*& left, void*& right, size_t num) {
		
		left = _freelist;
		right = left;

		for (size_t i = 0; i < num - 1; i++) {
			right = NextObj(right);

		}
		_freelist = NextObj(right);
		NextObj(right) = nullptr;
		_size -= num;
	}

	void Push(void* obj) {
		assert(obj);
		NextObj(obj) = _freelist;
		_freelist = obj;
		_size++;
	}

	void* Pop() {
		assert(_freelist);

		void* obj = _freelist;
		_freelist = NextObj(obj);
		_size--;
		return obj;
	}

	bool Empty() {
		return _freelist == nullptr;
	}

	size_t& MaxSize() {
		return _MaxSize;
	}

	size_t Size() {
		return _size;
	}

private:
	void* _freelist = nullptr;
	size_t _MaxSize = 1;
	size_t _size;
};

// ��������С�Ķ���ӳ�����
// ������������10%���ҵ�����Ƭ�˷�
// [1,128]					8byte����			freelist[0,16)
// [128+1,1024]				16byte����			freelist[16,72)
// [1024+1,8*1024]			128byte����			freelist[72,128)
// [8*1024+1,64*1024]		1024byte����		freelist[128,184)
// [64*1024+1,256*1024]		8*1024byte����		freelist[184,208)
class ClassAlignSize {
public:

	static size_t _AlignSize(size_t size, size_t align_num) {
		//std::cout << "_AlignSize is Success size : " << size << ", align_num : " << align_num << std::endl;
		//size_t alignSize;
		//if (size % align_num) {
		//	alignSize = (size / align_num + 1) * align_num;
		//}
		//else {
		//	alignSize = size / align_num;
		//}
		//return alignSize;
		return ((size + align_num - 1) & ~(align_num - 1));
	}

	// ���������
	static size_t AlignSize(size_t size) {
		//assert(size <= MAX_BYTES);

		if (size <= 8) {
			return _AlignSize(size, 8);
		}
		else if (size <= 1024) {
			return _AlignSize(size, 16);
		}
		else if (size <= 8 * 1024) {
			return _AlignSize(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _AlignSize(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _AlignSize(size, 8 * 1024);
		}
		else {
			return _AlignSize(size, 1 << PAGES_SHIFT);
		}
	}

	static size_t _Index(size_t size, size_t align_num) {
		size_t alignSize;
		//std::cout << "Index is fail ,size : " << size << std::endl;
		if (size % align_num) {
			alignSize = size % align_num - 1;
		}
		else {
			alignSize = size / align_num;
		}
		return alignSize;
	}

	// �ҵ�size���ĸ���ϣͰ
	static size_t Index(size_t size) {
		assert(size <= MAX_BYTES);

		static int group_bucket[4] = { 16,56,56,56 };

		if (size <= 128) {
			return _Index(size, 8);
		}
		else if (size <= 1024) {
			return _Index(size - 128, 16) + group_bucket[0];
		}
		else if (size <= 8 * 1024) {
			return _Index(size - 1024, 128) + group_bucket[1] + group_bucket[0];
		}
		else if (size <= 64 * 1024) {
			return _Index(size - 8 * 1024, 1024) + group_bucket[2] + group_bucket[1] + group_bucket[0];
		}
		else if (size <= 256 * 1024) {
			return _Index(size - 64 * 1024, 8 * 1024) + group_bucket[3] + group_bucket[2] + group_bucket[1] + group_bucket[0];
		}
		else {
			assert(false);
			return -1;
		}
	}

	// һ��thread cache��central cache�л�ȡ���ٸ�
	static size_t NumMoveSize(size_t size) {
		assert(size > 0);

		size_t num = MAX_BYTES / size;
		if (num < 2) {
			num = 2;
		}
		if (num > 512) {
			num = 512;
		}
		return num;
	}

	// ����һ����ϵͳ��ȡ����ҳ
	static size_t NumMovePage(size_t size) {
		assert(size > 0);
		size_t num = NumMoveSize(size);

		size_t page = num * size;

		page >>= PAGES_SHIFT;
		if (page == 0) page = 1;

		return page;
	}
};

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif // _WIN64

// ����������ҳ����ڴ��Ƚṹ
struct Span {
	Span()
		:_page_id(0), _page_num(0), _list(nullptr)
		, _use_count(0), _objsize(0), _is_exits(false)
	{
		_next = nullptr;
		_prev = nullptr;
	}

	PAGE_ID _page_id; // ҳ��
	size_t _page_num; // ҳ������

	Span* _next; // ˫����
	Span* _prev;

	void* _list; // ����ڴ���С��������
	size_t _use_count; // ʹ�ü���
	size_t _objsize; // �г����ĵ�������Ĵ�С

	bool _is_exits; // �Ƿ���ʹ��
};

// ����ҳ
class SpanList {
public:
	SpanList()
		:_head(new Span)
	{
		Init();
	}

	void Init() {
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin() {
		return _head->_next;
	}

	Span* End() {
		return _head;
	}

	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);
		Span* node = pos->_prev;
		node->_next = newSpan;
		newSpan->_prev = node;
		pos->_prev = newSpan;
		newSpan->_next = pos;
	}

	void Erase(Span* pos) {
		assert(pos);
		Span* node = pos->_next;
		pos->_prev->_next = node;
		node->_prev = pos->_prev;
	}

	bool Empty() {
		return _head->_next == _head;
	}

	void SpanLock() {
		_mutex.lock();
	}

	void SpanUnLock() {
		_mutex.unlock();
	}

private:
	Span* _head;
	std::mutex _mutex;
};