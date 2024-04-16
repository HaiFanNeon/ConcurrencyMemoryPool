#pragma once

#include <iostream>

/*
	定长内存池
*/

template <typename T>
class ObjectPool
{
public:
	T* New() {

		T* obj = nullptr;

		// 当自由链表中有空间
		if (_freeList)
		{
			void* ne = *(void**)_freeList;
			obj = (T*)_freeList;
			_freeList = ne;
		}
		else
		{
			// 当剩余的空间不够在开一个对象
			if (_sizeBytes < sizeof(T))
			{
				_sizeBytes = 128 * 1024;
				_memoryList = (char*)malloc(_sizeBytes);
				if (_memoryList == nullptr)
				{
					throw std::bad_alloc();
				}
			}


			obj = (T*)_memoryList;
			size_t objSize = sizeof(T) < sizeof(T*) ? sizeof(T*) : sizeof(T); // 如果说，T的大小<4/8个字节，那么就不能在T的前几个字节中存一个指针，来维护内存链表了。
			_memoryList += objSize;
			_sizeBytes -= objSize;
		}

		new(obj)T; // 定位new，来调用T的构造函数
		return obj;
	}



	void Delete(T* obj)
	{
		obj->~T(); // 显示调用析构函数，把空间还给内存池，自由链表接管这块空间


		// 使用头插来维护自由链表中的空间对象。
		if (_freeList == nullptr) // 当自由链表为空的时候进行头插
		{
			_freeList = obj; // 自由链表存obj的地址
			*(void**)obj = nullptr; // obj的前4/8个字节存nullptr，
		}
		else
		{
			*(void**)obj = _freeList;
			_freeList = obj;
		}
	}


private:
	char* _memoryList = nullptr; // 申请空间的链表
	void* _freeList = nullptr; // 自由链表
	size_t _sizeBytes = 0;
};