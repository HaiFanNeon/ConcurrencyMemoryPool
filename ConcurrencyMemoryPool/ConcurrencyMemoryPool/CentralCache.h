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
	{
		DelInstance();
	}
	CentralCache(const CentralCache& other) = delete;
	CentralCache& operator=(const CentralCache& other) = delete;
};

