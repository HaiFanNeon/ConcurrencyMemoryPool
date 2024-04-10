#pragma once

/*
	当我们申请完空间之后，使用一个链表来管理这块空间 --- 自由链表
	取空间的时候，直接在自由链表中进行头删
	释放空间的时候，直接在自由链表中头插
	但是申请空间的时候要申请的大小是不一样的，这就可以搞多个自由链表
	但并不用搞1到n个自由链表，只需要从2k，4k，8k，64k，128k 一直到256k即可。
	这样肯定会存在空间浪费的。比如说申请7字节，内存池会给你8字节，差值1字节就是内碎片。
	外碎片则是有足够的空间，但是这些空间不连续，不能进行申请。
*/

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

#include <cassert>

// 只能分配MAX_BYTES以下的空间
static const size_t MAX_BYTES = 256 * 1024;
static const size_t FREE_NUM = 206;
static const size_t PAGES_NUM = 128;
static const size_t PAGES_SHIFT = 13;

static void*& NextObj(void* obj) {
	return *(void**)obj;
}

class FreeList {
public:
	FreeList()
		:_freelist(nullptr),_MaxSize(1)
	{
	}

	// 批量插入
	void PushRange(void* left, void* right) {
		NextObj(right) = _freelist;
		_freelist = left;
	}

	void Push(void* obj) {
		assert(obj);
		NextObj(obj) = _freelist;
		_freelist = obj;
	}

	void* Pop() {
		assert(_freelist);
		void* obj = _freelist;
		_freelist = NextObj(obj);
		return obj;
	}

	bool Empty() {
		return _freelist == nullptr;
	}

	size_t& MaxSize() {
		return _MaxSize;
	}
private:
	void* _freelist;
	size_t _MaxSize;
};

// 计算对象大小的对齐映射规则
// 整体控制在最多10%左右的内碎片浪费
// [1,128]					8byte对齐			freelist[0,16)
// [128+1,1024]				16byte对齐			freelist[16,72)
// [1024+1,8*1024]			128byte对齐			freelist[72,128)
// [8*1024+1,64*1024]		1024byte对齐		freelist[128,184)
// [64*1024+1,256*1024]		8*1024byte对齐		freelist[184,208)
class ClassAlignSize {
public:

	static size_t _AlignSize(size_t size, size_t align_num) {
		std::cout << "_AlignSize is Success size : " << size << ", align_num : " << align_num << std::endl;
		size_t alignSize;
		if (size % align_num) {
			alignSize = (size / align_num + 1) * align_num;
		}
		else {
			alignSize = size / align_num;
		}
		return alignSize;
	}

	// 计算对齐数 
	static size_t AlignSize(size_t size) {
		assert(size <= MAX_BYTES);

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
			std::cout << "AlignSize is fail ,size : " << size << std::endl;
			return -1;
		}
	}


	static size_t _Index(size_t size, size_t align_num) {
		size_t alignSize;

		if (size % align_num) {
			alignSize = size / align_num - 1;
		}
		else {
			alignSize = size / align_num;
		}
		return alignSize;
	}

	// 找到size是哪个哈希桶
	static size_t Index(size_t size) {

		assert(size <= MAX_BYTES);

		static int group_bucket[4] = { 16,56,56,56 };

		if (size <= 8) {

			return _AlignSize(size, 8);
		}
		else if (size <= 16) {
			return _AlignSize(size - 128, 16) + group_bucket[0];
		}
		else if (size <= 128) {
			return _AlignSize(size - 1024, 128) + group_bucket[1] + group_bucket[0];
		}
		else if (size <= 1024) {
			return _AlignSize(size - 8 * 1024, 1024) + group_bucket[2] + group_bucket[1] + group_bucket[0];
		}
		else if (size <= 8 * 1024) {
			return _AlignSize(size - 64 * 1024, 8 * 1024) + group_bucket[3] + group_bucket[2] + group_bucket[1] + group_bucket[0];
		}
		else {
			std::cout << "AlignSize is fail ,size : " << size << std::endl;
			return -1;
		}
	}

	// 一次thread cache从central cache中获取多少个
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

	// 计算一次向系统获取几个页
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

// 管理多个连续页大块内存跨度结构
struct Span {

	Span()
		:_page_id(0),_page_num(0),_next(nullptr),_prev(nullptr)
		,_list(nullptr),_use_count(0),_objsize(0),_is_exits(false)
	{}

	PAGE_ID _page_id; // 页号
	size_t _page_num; // 页的数量

	Span* _next; // 双链表
	Span* _prev;

	void* _list; // 大块内存切小链接起来
	size_t _use_count; // 使用计数
	size_t _objsize; // 切出来的单个对象的大小

	bool _is_exits; // 是否在使用
};

// 管理页
class SpanList {
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin() {
		return _head->_next;
	}

	Span* End() {
		return _head;
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