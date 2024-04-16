#pragma once


#include "Common.hpp"
#include "ObjectPool.h"
#include "PageMap.hpp"


// ����ģʽ
class PageCache {
public:
	static PageCache _pageCache;
	static PageCache* GetInstance();
	//static void DelInstance();

	// ��ȡһ��kҳ��span
	Span* GetKSpan(size_t k);

	void PageLock() {
		_page_mutex.lock();
	}

	void PageUnLock() {
		_page_mutex.unlock();
	}

	// ��ȡ�Ӷ���span��ӳ���ϵ
	Span* MapObjectToSpan(void* obj);
	// �ͷſ���span�ص�PageCache�����ϲ����ڵ�span
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

