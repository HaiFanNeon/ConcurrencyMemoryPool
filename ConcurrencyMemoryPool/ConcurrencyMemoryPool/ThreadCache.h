#pragma once

#include "Common.hpp"

class ThreadCache {
public:
	// ������ͷſռ�
	void* Allocate(size_t size);
	void Deallocate(void *ptr, size_t size);

	// ��ThreadCache�ڴ治���ʱ����Ҫ�����Ļ�������
	void* FromCentralFetch(size_t index, size_t alignSize);

	void ListTooLong(FreeList& list, size_t size);
private:

	FreeList _freelists[NFREELIST];

};

thread_local static ThreadCache* pTLSthreadCache = nullptr;