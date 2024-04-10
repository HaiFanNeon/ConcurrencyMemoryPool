# concurrency memory pool
基于谷歌开源的tcmalloc简版
# 项目简介
连续的申请空间是很耗时的，内存池可以预先的向操作系统中申请出来一大块内存，当程序需要内存的时候，向内存池申请即可。释放内存的时候，并不真正的将内存还给操作系统，而是还给内存池。当程序退出或者在特定的时候的时候，才会把内存池中的内存还给操作系统。
# 高并发内存池整体框架设计
Concurrent memory pool主要由以下三个部分组成
1. thread cache：线程缓存时每个线程独有的，用于小于256kb的内存的分配，线程从这里申请内存不需要加锁，每个线程独享一个cache，这也就是这个内存池高效的地方
2. central cache：中心缓存是所有线程所共享，thread cache是按需从central cache中获取的对象。central cache合适的时机会收回thread cache中的对象，避免一个线程占用了太多的内存，而其他线程的内存吃紧，达到内存分配在多个线程中更均衡的按序调度的目的。central cache是存在竞争的，所以从这里取内存对象是需要加锁的，首先这里用的是桶锁，其次只有thread cache的没有内存对象才会找central cache，所以这里竞争不会很激烈
3. page cache：页缓存是在central cache缓存上面的一层缓存，存储的内存是以页为单位存储及分配的，central cache没有内存对象时，从page cache分配出一定数量的page，并切割成定长大小的小块内存，分配给central cache。当一个span的几个跨度页的对象都回收以后，page cache会回收central cache满足条件的span对象，并且合并相邻的页，组成更大的页，缓解内存碎片的问题
![[Pasted image 20240407195223.png]]

# Common.hpp
这个hpp文件用于实现公共的类以及函数和变量部分
```CPP
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

```
# thread cache
申请内存
1. 当内存申请size <= 256kb，先获取到线程本地存储的thread cache对象，计算size映射的哈希桶自由链表下标index
2. 如果自由链表_freeLists[index]中有对象，则直接Pop一个内存对象返回
3. 如果_freeLists[i]中没有对象时，则批量从central cache中获取一定数量的对象，插入到自由链表并返回一个对象
释放内存
1. 当释放内存小于256k时将内存释放回thread cache，计算size映射自由链表桶位置i，将对象Push  到_freeLists[i]。
2. 当链表的长度过长，则回收一部分内存对象到central cache。
---
代码架构
```cpp
class ThreadCache {
public:
	// 申请和释放空间
	void* Allocate(size_t size);
	void Deallocate(void *ptr, size_t size);

	// 当ThreadCache内存不足的时候，需要向中心缓存申请
	void* FromCentralFetch(size_t index, size_t alignSize);
private:

	FreeList _freelists[FREE_NUM];

};

// 线程局部变量
thread_local static ThreadCache* pTLSthreadCache = nullptr;
```
# Central Cache
central cache也是一个哈希桶结构，他的哈希桶的映射关系跟thread cache是一样的。不同的是每个哈希桶位置挂的是SpalList链表结构，每个映射桶下面的span中的大内存块被映射关系切成了一个个小内存块对象挂在span的自由链表中。
![[Pasted image 20240409194338.png]]
## 申请内存
1. 当thread cache中没有内存的时候，会批量向central cache申请一些内存对象，这里的批量获取对像的数量使用了类似网络tcp协议拥塞控制的慢开始算法；central cache也有一个哈希映射的spanlist，spanlist中挂着span，从span中取出对象给thread cache，这个过程是需要加锁的，不过这里使用的是一个桶锁，尽可能提高效率。
2. central cache映射的spanlist中所有span都没有内存后，则需要像page cache申请一个新span对象，拿到span以后将span管理的内存按大小切好作为自由链表链接到一起，然后从span中取对象给thread cache。
3. central cache中挂的span中use_count记录分配了多少个对象出去，分配一个对象给thread cache就++use_count
## 释放内存
1. 当thread cache过长或者线程销毁，则会将内存释放回central cache中，释放回来时--use_count，当use_count减到0时则表示所有对象都回到了span，则将span释放回page cache，page cache中会对前后相邻的空闲页进行合并。


---

```CPP
#pragma once
#include "Common.hpp"

class CentralCache {
public:
	
	// 单例模式
	static CentralCache* GetInstance();
	// 在进程结束的时候，释放资源
	static void DelInstance();

	// 申请内存
	size_t FetchRangeObj(void*& left, void*& right, size_t num, size_t size);

	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t byte_size);
private:
	SpanList _spanlists[FREE_NUM];
	static CentralCache* _centralCache;

private:
	CentralCache()
	{}
	~CentralCache()
	{}
	CentralCache(const CentralCache& other) = delete;
	CentralCache& operator=(const CentralCache& other) = delete;
};

```

# Page Cache
## 申请内存
1. 当central cache向page cache申请内存时，page cache先检查对应位置有没有span，如果没有  则向更大页寻找一个span，如果找到则分裂成两个。比如：申请的是4页page，4页page后面没  有挂span，则向后面寻找更大的span，假设在10页page位置找到一个span，则将10页page  span分裂为一个4页page span和一个6页page span。
2. 如果找到_spanList[128]都没有合适的span，则向系统使用mmap、brk或者是VirtualAlloc等方式  申请128页page span挂在自由链表中，再重复1中的过程。
3. 需要注意的是central cache和page cache 的核心结构都是spanlist的哈希桶，但是他们是有本质  区别的，central cache中哈希桶，是按跟thread cache一样的大小对齐关系映射的，他的spanlist  中挂的span中的内存都被按映射关系切好链接成小块内存的自由链表。而page cache 中的  spanlist则是按下标桶号映射的，也就是说第i号桶中挂的span都是i页内存。
## 释放内存
1. 如果central cache释放回一个span，则依次寻找span的前后page id的没有在使用的空闲span，  看是否可以合并，如果合并继续向前寻找。这样就可以将切小的内存合并收缩成大的span，减少内存碎片。![[Pasted image 20240410185047.png]]

```CPP

```
