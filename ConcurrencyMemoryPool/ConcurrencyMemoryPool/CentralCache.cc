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

	// 从span中拿内存，如果不够，有多少拿多少
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
	// 查看当前的SpanList中是否还有未分配对象的span

	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_list != nullptr) {
			return it;
		}
		else 
			it = it->_next;
	}

	// 走到这里说明spanlist中已经没有对象了，需要向page cache申请
	Span* span = PageCache::GetInstance()->GetKSpan(ClassAlignSize::NumMovePage(byte_size));
	// 计算span的大块内存的起始地址和大块内存的大小
	void* left = (void*)(span->_page_id << PAGES_SHIFT);
	size_t bytes = span->_page_num << PAGES_SHIFT;

	// 把大块内存切成自由链表链接起来
	// 1. 

	return nullptr;
}