#pragma once

#include "Common.hpp"

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

thread_local static ThreadCache* pTLSthreadCache = nullptr;