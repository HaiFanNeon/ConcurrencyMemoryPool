//#include "Common.hpp"
//#include "CurrentAllocate.h"
//#include "CentralCache.h"
//#include "PageCache.h"
//#include "ThreadCache.h"
//
//void test1() {
//	for (int i = 0; i < 5; i++) {
//		void* ptr = CurrentAlloc(7);
//	}
//}
//
//void test2() {
//	for (int i = 0; i < 5; i++) {
//		void* ptr = CurrentAlloc(7);
//	}
//}
//
//
//
//void test3() {
//	std::vector<void*> st;
//	st.push_back(0);
//	for (int i = 1; i <= 1000; i++) {
//		void* ptr = CurrentAlloc((i + 1) % 8192);
//		st.push_back(ptr);
//		std::cout << ptr << std::endl;
//	}
//	for (int i = 1; i <= 100; i++) {
//		void* t = st[i];
//		CurrentFree(t);
//		
//	}
//
//
//	//void* ptr = CurrentAlloc(9);
//	//std::cout << ptr << std::endl;
//
//}
//
//void test4() {
//	std::vector<void*> st;
//	for (int i = 1; i <= 100; i++) {
//		void* ptr = CurrentAlloc((i + 1) % 8192);
//		st.push_back(ptr);
//		std::cout << ptr << std::endl;
//	}
//	for (int i = 1; i <= 100; i++) {
//		
//		void* t = st[i - 1];
//		CurrentFree(t);
//
//	}
//
//}
//
//void test5() {
//	std::vector<void*> st;
//	for (int i = 1; i <= 100; i++) {
//		void* ptr = CurrentAlloc((i + 1) % 8192);
//		st.push_back(ptr);
//		std::cout << ptr << std::endl;
//	}
//	for (int i = 1; i <= 100; i++) {
//
//		void* t = st[i - 1];
//		CurrentFree(t);
//
//	}
//}
//
//
//
//void test() {
//	std::thread t1(test3);
//	//std::thread t2(test4);
//	//std::thread t3(test5);
//
//	t1.join();
//	//t2.join();
//	//t3.join();
//
//}
//
//int main()
//{
//	test();
//	return 0;
//}