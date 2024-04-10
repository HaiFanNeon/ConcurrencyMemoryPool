#pragma once
#include "Common.hpp"

class CentralCache {
public:
	
	// ����ģʽ
	static CentralCache* GetInstance();
	// �ڽ��̽�����ʱ���ͷ���Դ
	static void DelInstance();

	// �����ڴ�
	size_t FetchRangeObj(void*& left, void*& right, size_t num, size_t size);

	// ��ȡһ���ǿյ�span
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

