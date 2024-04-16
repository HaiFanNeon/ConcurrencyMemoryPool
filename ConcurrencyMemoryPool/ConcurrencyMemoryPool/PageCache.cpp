#include "PageCache.h"


//PageCache* PageCache::_pageCache = nullptr;
PageCache PageCache::_pageCache;

PageCache* PageCache::GetInstance() {
	//if (_pageCache == nullptr) {
	//	_pageCache = new PageCache();
	//}
	return& _pageCache;
}

//void PageCache::DelInstance() {
//	if (_pageCache == nullptr) {
//		return;
//	}
//	delete _pageCache;
//	_pageCache = nullptr;
//}

// ��ȡһ��kҳ��span
Span* PageCache::GetKSpan(size_t k) {
	//assert(k > 0 && k < PAGES_NUM);


	
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_page_id = (PAGE_ID)ptr >> PAGES_SHIFT;
		span->_page_num = k;

		//_idSpanMap[span->_page_id] = span;
		_idSpanMap.set(span->_page_id, span);

		return span;
	}

	if (!_spanLists[k].Empty()) {
		Span* kSpan =  _spanLists[k].PopFront();


		for (PAGE_ID i = 0; i < kSpan->_page_num; i++) {
			//_idSpanMap[kSpan->_page_id + i] = kSpan;
			_idSpanMap.set(kSpan->_page_id + i, kSpan);
		}

		return kSpan;
	}

	// �зֳ�һ��kҳ��span��һ��n - kҳ��span��kҳ��span���ظ�central cache
	// ʣ���n - kҳ��span�ҵ���n - k Ͱ��ȥ
	for (size_t i = k; i < NPAGES; i++) {
		if (!_spanLists[i].Empty()) {
			Span* nSpan = _spanLists[i].PopFront();

			// Span* kSpan = new span;

			// ��Ϊ�����ڴ���е�New
			Span* kSpan = _spanPool.New();

			// ��nSpan��ͷ����һ��kҳ����
			kSpan->_page_id = nSpan->_page_id;
			kSpan->_page_num = k;

			nSpan->_page_id += k;
			nSpan->_page_num -= k;

			_spanLists[nSpan->_page_num].PushFront(nSpan);

			// �洢nSapn����λҳ�Ÿ�nSpanӳ�䣬����page cache�����ڴ�
			// ���еĺϲ�����
			//_idSpanMap[nSpan->_page_id] = nSpan;
			//_idSpanMap[nSpan->_page_id + nSpan->_page_num - 1] = nSpan;

			_idSpanMap.set(nSpan->_page_id, nSpan);
			_idSpanMap.set(nSpan->_page_id + nSpan->_page_num - 1, nSpan);

			// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID j = 0; j < kSpan->_page_num; j++) {
				/*_idSpanMap[kSpan->_page_id + j] = kSpan;*/
				_idSpanMap.set(kSpan->_page_id + j, kSpan);
			}

			return kSpan;
		}
	}

	// �ߵ����λ�þ�˵������û�д�ҳ��span��
	// ��ʱ��ȥ�Ҷ�Ҫһ��128ҳ��span

	// Span*bigSpan = new Span();
	// ʹ�ö����ڴ���е�New
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_page_id = (PAGE_ID)ptr >> PAGES_SHIFT;
	bigSpan->_page_num = NPAGES - 1;

	_spanLists[bigSpan->_page_num].PushFront(bigSpan);
	return GetKSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	PAGE_ID id = ((PAGE_ID)obj >> PAGES_SHIFT);

	//std::unique_lock<std::mutex> lock(_page_mutex);
	//auto it = _idSpanMap.find(id);
	//if (it == _idSpanMap.end()) {
	//	assert(nullptr);
	//}
	//

	auto it = (Span*)_idSpanMap.get(id);
	assert(it);
	return it;
	
}

void PageCache::ReleaseSpanToPageCache(Span* span) {

	// ����128ҳ�ģ�ֱ�ӻ�����
	if (span->_page_num > NPAGES - 1) {
		void* ptr = (void*)(span->_page_id >> PAGES_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	// ��spanǰ���ҳ�� ���Խ��кϲ��������ڴ���Ƭ����
	while (true) {
		PAGE_ID prevId = span->_page_id - 1;
		//auto it = _idSpanMap.find(prevId);
		//// ǰ���ҳ��û�У����ϲ���
		//if (it == _idSpanMap.end()) {
		//	break;
		//}

		auto it = (Span*)_idSpanMap.get(prevId);
		if (it == nullptr) {
			break;
		}

		// ǰ������ҳ��span��ʹ�ã����ϲ���
		Span* prevSpan = it;
		if (prevSpan->_is_exits == true) {
			break;
		}

		// �ϲ�������128ҳ��spanû�취���������кϲ�
		if (prevSpan->_page_num + span->_page_num > NPAGES - 1) {
			break;
		}

		span->_page_id = prevSpan->_page_id;
		span->_page_num += prevSpan->_page_num;

		_spanLists[prevSpan->_page_num].Erase(prevSpan); 

		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	while (true) {
		PAGE_ID nextid = span->_page_id + span->_page_num - 1;
		//auto it = _idSpanMap.find(nextid);
		//Span* nextSpan = it->second;
		//if (it == _idSpanMap.end()) {
		//	break;
		//}

		auto it = (Span*)_idSpanMap.get(nextid);
		if (it == nullptr) {
			break;
		}

		Span* nextSpan = it;
		if (nextSpan->_is_exits == true) {
			break;
		}

		if (nextSpan->_page_num + span->_page_num > NPAGES - 1) {
			break;
		}

		span->_page_num += nextSpan->_page_num;
		_spanLists[nextSpan->_page_num].Erase(nextSpan);

		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	_spanLists[span->_page_num].PushFront(span);
	span->_is_exits = false;
	//_idSpanMap[span->_page_id] = span;
	//_idSpanMap[span->_page_id + span->_page_num - 1] = span;
	_idSpanMap.set(span->_page_id, span);
	_idSpanMap.set(span->_page_id + span->_page_num - 1, span);
}