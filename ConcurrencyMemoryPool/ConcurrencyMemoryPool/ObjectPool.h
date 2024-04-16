#pragma once

#include <iostream>

/*
	�����ڴ��
*/

template <typename T>
class ObjectPool
{
public:
	T* New() {

		T* obj = nullptr;

		// �������������пռ�
		if (_freeList)
		{
			void* ne = *(void**)_freeList;
			obj = (T*)_freeList;
			_freeList = ne;
		}
		else
		{
			// ��ʣ��Ŀռ䲻���ڿ�һ������
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
			size_t objSize = sizeof(T) < sizeof(T*) ? sizeof(T*) : sizeof(T); // ���˵��T�Ĵ�С<4/8���ֽڣ���ô�Ͳ�����T��ǰ�����ֽ��д�һ��ָ�룬��ά���ڴ������ˡ�
			_memoryList += objSize;
			_sizeBytes -= objSize;
		}

		new(obj)T; // ��λnew��������T�Ĺ��캯��
		return obj;
	}



	void Delete(T* obj)
	{
		obj->~T(); // ��ʾ���������������ѿռ仹���ڴ�أ���������ӹ����ռ�


		// ʹ��ͷ����ά�����������еĿռ����
		if (_freeList == nullptr) // ����������Ϊ�յ�ʱ�����ͷ��
		{
			_freeList = obj; // ���������obj�ĵ�ַ
			*(void**)obj = nullptr; // obj��ǰ4/8���ֽڴ�nullptr��
		}
		else
		{
			*(void**)obj = _freeList;
			_freeList = obj;
		}
	}


private:
	char* _memoryList = nullptr; // ����ռ������
	void* _freeList = nullptr; // ��������
	size_t _sizeBytes = 0;
};