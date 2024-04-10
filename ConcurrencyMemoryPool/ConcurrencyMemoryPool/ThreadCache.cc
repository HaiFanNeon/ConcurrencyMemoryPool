#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
	// 1. size�Ĵ�СӦ��С��MAX_BYTES
	assert(size <= MAX_BYTES);

	size_t alignSize = ClassAlignSize::AlignSize(size);
	size_t index = ClassAlignSize::Index(size);

	void* obj = nullptr;

	if (!_freelists[index].Empty()) {
		obj = _freelists[index].Pop();
	}
	else {
		FromCentralFetch(index, size);
	}

	return obj;
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(size <= MAX_BYTES);
	assert(ptr);

	size_t alignSize = ClassAlignSize::AlignSize(size);
	size_t index = ClassAlignSize::Index(size);

	_freelists[index].Push(ptr);
}

void* ThreadCache::FromCentralFetch(size_t index, size_t size) {

	// ����ʼ���������㷨
	// 1. �ʼ����һ���Ե���central cacheһ������Ҫ̫�࣬
	// 2. batchNum�᲻������
	size_t batchNum = std::min(_freelists[index].MaxSize(), \
		ClassAlignSize::NumMoveSize(size));

	if (batchNum == _freelists[index].MaxSize()) {
		// ������ñ仯����������+2��+3��+4��
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
		_freelists[index].PushRange(NextObj(left), right);
		return left;
	}

	return nullptr;
}