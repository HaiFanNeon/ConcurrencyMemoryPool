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

	// 从span中拿内存，如果不够，有多少拿多少
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
	// 查看当前的SpanList中是否还有未分配对象的span

	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_list != nullptr) {
			return it;
		}
		else
			it = it->_next;
	}
	// 先把central cache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
	list.SpanUnLock();
	// 走到这里说明spanlist中已经没有对象了，需要向page cache申请
	PageCache::GetInstance()->PageLock();
	Span* span = PageCache::GetInstance()->GetKSpan(ClassAlignSize::NumMovePage(byte_size));
	span->_is_exits = true;
	span->_objsize = byte_size;
	PageCache::GetInstance()->PageUnLock();

	// 计算span的大块内存的起始地址和大块内存的大小
	char* left = (char*)(span->_page_id << PAGES_SHIFT);
	size_t bytes = span->_page_num << PAGES_SHIFT;
	char* right = left + bytes;

	// 把大块内存切成自由链表链接起来
	// 1. 先切一块下来去做头，方便尾插
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

		// 说明span切分出去的所有小块内存都回来了
		span->_use_count--;
		if (span->_use_count == 0) {
			_spanlists[index].Erase(span);
			span->_list = nullptr;
			span->_next = span->_prev = nullptr;

			// 释放span给page cache时，使用page cache的锁就可以了
			// 这时把桶锁解掉
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