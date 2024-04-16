#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
	// 1. size的大小应该小于MAX_BYTES
	assert(size <= MAX_BYTES);

	size_t alignSize = ClassAlignSize::AlignSize(size);
	size_t index = ClassAlignSize::Index(size);

	void* obj = nullptr;

	if (!_freelists[index].Empty()) {
		obj = _freelists[index].Pop();
	}
	else {
		obj = FromCentralFetch(index, size);
	}

	return obj;
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(size <= MAX_BYTES);
	assert(ptr);

	size_t alignSize = ClassAlignSize::AlignSize(size);
	size_t index = ClassAlignSize::Index(size);
	_freelists[index].Push(ptr);

	// 当链表的长度大于一次批量申请的内存时，就开始还一段list给central cache
	if (_freelists[index].Size() >= _freelists[index].MaxSize()) {
		ListTooLong(_freelists[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size) {
	void* left = nullptr;
	void* right = nullptr;
	list.PopRange(left, right, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(left, size);
}

void* ThreadCache::FromCentralFetch(size_t index, size_t size) {

	// 慢开始反馈调节算法
	// 1. 最开始不会一次性的向central cache一次批量要太多，
	// 2. batchNum会不断增长
	size_t batchNum = std::min<size_t>(_freelists[index].MaxSize(), ClassAlignSize::NumMoveSize(size));

	if (batchNum == _freelists[index].MaxSize()) {
		// 如果觉得变化的慢，可以+2，+3，+4等
		batchNum += 1;
	}

	void* left = nullptr;
	void* right = nullptr;

	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(left, right, batchNum, size);
	//assert(actualNum > 1);

	if (actualNum == 1) {
		assert(left == right);
		return left;
	}
	else {
		_freelists[index].PushRange(NextObj(left), right, actualNum);
		return left;
	}

	return nullptr;
}