#pragma once

#include "Common.hpp"
#include "ThreadCache.h"
#include "PageCache.h"
#include "CentralCache.h"


static void* CurrentAlloc(size_t size) {

	if (size > MAX_BYTES) {
		size_t alignSize = ClassAlignSize::AlignSize(size);
		size_t kpage = alignSize >> PAGES_SHIFT;

		PageCache::GetInstance()->PageLock();
		Span* span = PageCache::GetInstance()->GetKSpan(kpage);
		span->_objsize = size;
		PageCache::GetInstance()->PageUnLock();

		void* ptr = (void*)(span->_page_id << PAGES_SHIFT);

		return ptr;
	}
	else {
		//size_t alignSize = ClassAlignSize::AlignSize(size);
		//size_t index = ClassAlignSize::Index(size);

		if (pTLSthreadCache == nullptr) {
			static ObjectPool<ThreadCache> tcPool;
			//pTLSthreadCache = new ThreadCache;
			pTLSthreadCache = tcPool.New();
		}


		//std::cout << pTLSthreadCache << ":" << std::this_thread::get_id() << std::endl;

		return pTLSthreadCache->Allocate(size);
	}
}

static void CurrentFree(void* ptr) {

	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;



	if (size > MAX_BYTES) {
		

		PageCache::GetInstance()->PageLock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->PageUnLock();
	}
	else {
		pTLSthreadCache->Deallocate(ptr, size);
	}
}