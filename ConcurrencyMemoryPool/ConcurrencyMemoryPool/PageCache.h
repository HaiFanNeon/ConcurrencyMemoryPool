#pragma once


#include "Common.hpp"
#include "ObjectPool.h"
#include "PageMap.hpp"


// 单例模式
class PageCache {
public:
	static PageCache _pageCache;
	static PageCache* GetInstance();
	//static void DelInstance();

	// 获取一个k页的span
	Span* GetKSpan(size_t k);

	void PageLock() {
		_page_mutex.lock();
	}

	void PageUnLock() {
		_page_mutex.unlock();
	}

	// 获取从对象到span的映射关系
	Span* MapObjectToSpan(void* obj);
	// 释放空闲span回到PageCache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
	
private:
	SpanList _spanLists[NPAGES];
	std::mutex _page_mutex;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;

	TCMalloc_PageMap1<32 - PAGES_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;

	
private:
	PageCache() {}
	~PageCache() {
		//DelInstance();
	}
	const PageCache& operator= (const PageCache& other) = delete;
	PageCache(const PageCache& other) = delete;
};

