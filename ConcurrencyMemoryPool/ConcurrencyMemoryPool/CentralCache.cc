#include "CentralCache.h"
#include "PageCache.h"

//CentralCache* CentralCache::_centralCache = nullptr;
CentralCache CentralCache::_centralCache;

CentralCache* CentralCache::GetInstance() {
	return&_centralCache;
}

//void CentralCache::DelInstance() {
//	delete _centralCache;
//	_centralCache = nullptr;
//}

size_t CentralCache::FetchRangeObj(void*& left, void*& right, size_t batchNum, size_t size) {
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
	while (i < batchNum - 1 && NextObj(right) != nullptr) {
		right = NextObj(right);
		++i;
		++actualNum;
	}
	getSpan->_list = NextObj(right);
	NextObj(right) = nullptr;
	getSpan->_use_count += actualNum;
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
	// �Ȱ�central cache��Ͱ�������������������߳��ͷ��ڴ�����������������
	list.SpanUnLock();
	// �ߵ�����˵��spanlist���Ѿ�û�ж����ˣ���Ҫ��page cache����
	PageCache::GetInstance()->PageLock();
	Span* span = PageCache::GetInstance()->GetKSpan(ClassAlignSize::NumMovePage(byte_size));
	span->_is_exits = true;
	span->_objsize = byte_size;
	PageCache::GetInstance()->PageUnLock();

	// ����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С
	char* left = (char*)(span->_page_id << PAGES_SHIFT);
	size_t bytes = span->_page_num << PAGES_SHIFT;
	char* right = left + bytes;

	// �Ѵ���ڴ��г�����������������
	// 1. ����һ������ȥ��ͷ������β��
	span->_list = left;
	left += bytes;
	void* tail = span->_list;

	while (left < right) {
		NextObj(tail) = left;
		tail = NextObj(tail);
		left += byte_size;
	}

	NextObj(tail) = nullptr;

	list.SpanLock();
	list.PushFront(span);

	return span;
}

void CentralCache::ReleaseListToSpans(void* left, size_t size) {
	size_t index = ClassAlignSize::Index(size);
	_spanlists[index].SpanLock();
	while (left) {
		void* next = NextObj(left);

		//Span* span =
		Span* span = PageCache::GetInstance()->MapObjectToSpan(left);

		NextObj(left) = span->_list;
		span->_list = left;

		// ˵��span�зֳ�ȥ������С���ڴ涼������
		span->_use_count--;
		if (span->_use_count == 0) {
			_spanlists[index].Erase(span);
			span->_list = nullptr;
			span->_next = span->_prev = nullptr;

			// �ͷ�span��page cacheʱ��ʹ��page cache�����Ϳ�����
			// ��ʱ��Ͱ�����
			_spanlists[index].SpanUnLock();

			PageCache::GetInstance()->PageLock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->PageUnLock();

			_spanlists[index].SpanLock();
		}

		left = next;
	}
	_spanlists[index].SpanUnLock();
}