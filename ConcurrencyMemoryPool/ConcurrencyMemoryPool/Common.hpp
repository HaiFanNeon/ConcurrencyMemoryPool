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

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

#include <cassert>

// ֻ�ܷ���MAX_BYTES���µĿռ�
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

	// ��������
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

	// ��������� 
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

	// �ҵ�size���ĸ���ϣͰ
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
		:_page_id(0),_page_num(0),_next(nullptr),_prev(nullptr)
		,_list(nullptr),_use_count(0),_objsize(0),_is_exits(false)
	{}

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