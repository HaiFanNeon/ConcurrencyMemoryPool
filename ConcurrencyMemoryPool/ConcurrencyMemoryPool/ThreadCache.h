#pragma once

#include "Common.hpp"

class ThreadCache {
public:
	// ������ͷſռ�
	void* Allocate(size_t size);
	void Deallocate(void *ptr, size_t size);

	// ��ThreadCache�ڴ治���ʱ����Ҫ�����Ļ�������
	void* FromCentralFetch(size_t index, size_t alignSize);
private:

	FreeList _freelists[FREE_NUM];

};

thread_local static ThreadCache* pTLSthreadCache = nullptr;