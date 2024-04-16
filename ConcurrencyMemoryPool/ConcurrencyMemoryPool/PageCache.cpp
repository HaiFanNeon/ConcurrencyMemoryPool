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

// 获取一个k页的span
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

	// 切分成一个k页的span和一个n - k页的span，k页的span返回给central cache
	// 剩余的n - k页的span挂到第n - k 桶中去
	for (size_t i = k; i < NPAGES; i++) {
		if (!_spanLists[i].Empty()) {
			Span* nSpan = _spanLists[i].PopFront();

			// Span* kSpan = new span;

			// 改为定长内存池中的New
			Span* kSpan = _spanPool.New();

			// 在nSpan的头部切一个k页下来
			kSpan->_page_id = nSpan->_page_id;
			kSpan->_page_num = k;

			nSpan->_page_id += k;
			nSpan->_page_num -= k;

			_spanLists[nSpan->_page_num].PushFront(nSpan);

			// 存储nSapn的首位页号跟nSpan映射，方便page cache回收内存
			// 进行的合并查找
			//_idSpanMap[nSpan->_page_id] = nSpan;
			//_idSpanMap[nSpan->_page_id + nSpan->_page_num - 1] = nSpan;

			_idSpanMap.set(nSpan->_page_id, nSpan);
			_idSpanMap.set(nSpan->_page_id + nSpan->_page_num - 1, nSpan);

			// 简历id和span的映射，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID j = 0; j < kSpan->_page_num; j++) {
				/*_idSpanMap[kSpan->_page_id + j] = kSpan;*/
				_idSpanMap.set(kSpan->_page_id + j, kSpan);
			}

			return kSpan;
		}
	}

	// 走到这个位置就说明后面没有大页的span了
	// 这时就去找堆要一个128页的span

	// Span*bigSpan = new Span();
	// 使用定长内存池中的New
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

	// 大于128页的，直接还给堆
	if (span->_page_num > NPAGES - 1) {
		void* ptr = (void*)(span->_page_id >> PAGES_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	// 对span前后的页， 尝试进行合并，缓解内存碎片问题
	while (true) {
		PAGE_ID prevId = span->_page_id - 1;
		//auto it = _idSpanMap.find(prevId);
		//// 前面的页号没有，不合并了
		//if (it == _idSpanMap.end()) {
		//	break;
		//}

		auto it = (Span*)_idSpanMap.get(prevId);
		if (it == nullptr) {
			break;
		}

		// 前面相邻页的span在使用，不合并了
		Span* prevSpan = it;
		if (prevSpan->_is_exits == true) {
			break;
		}

		// 合并出超过128页的span没办法管理，不进行合并
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