#include "CentralCache.h"
#include "PageCache.h"

CentralCache* CentralCache::_centralCache = nullptr;

CentralCache* CentralCache::GetInstance() {
	if (_centralCache == nullptr) {
		_centralCache = new CentralCache();
	}

	return _centralCache;
}

void CentralCache::DelInstance() {
	delete _centralCache;
	_centralCache = nullptr;
}

size_t CentralCache::FetchRangeObj(void*& left, void*& right, size_t num, size_t size) {
	
	size_t index = ClassAlignSize::Index(size);
	_spanlists[index].SpanLock();

	Span* getSpan = CentralCache::GetInstance()->GetOneSpan(_spanlists[index], size);
	assert(getSpan);
	assert(getSpan->_list);

	// ��span�����ڴ棬����������ж����ö���
	left = getSpan->_list;
	right = left;
	size_t i = 0;
	size_t actualNum = 1;
	while (i < num - 1 && NextObj(right) != nullptr) {
		right = NextObj(right);
		++i;
		++actualNum;
	}
	getSpan->_list = NextObj(right);
	NextObj(right) = nullptr;

	_spanlists[index].SpanUnLock();
	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size) {
	// �鿴��ǰ��SpanList���Ƿ���δ��������span

	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_list != nullptr) {
			return it;
		}
		else 
			it = it->_next;
	}

	// �ߵ�����˵��spanlist���Ѿ�û�ж����ˣ���Ҫ��page cache����
	Span* span = PageCache::GetInstance()->GetKSpan(ClassAlignSize::NumMovePage(byte_size));
	// ����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С
	void* left = (void*)(span->_page_id << PAGES_SHIFT);
	size_t bytes = span->_page_num << PAGES_SHIFT;

	// �Ѵ���ڴ��г�����������������
	// 1. 

	return nullptr;
}