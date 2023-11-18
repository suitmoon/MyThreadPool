
#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"
#include <memory>


using uLong_ = unsigned long long;
class MyTask :public Task {
public:
	MyTask(uLong_ begin, uLong_ end) :begin_(begin), end_(end) {}
	Any run() {
		uLong_ sum_ = 0;
		for (uLong_ i = begin_; i <= end_; i++)
		{
			sum_ += i;
		}
		return sum_;
	}
private:
	uLong_ begin_;
	uLong_ end_;

};


int main()
{
	{	
		uLong_ res = 0;
		std::chrono::milliseconds duration;	
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_FIXED);
		pool.start();
		auto start = std::chrono::high_resolution_clock::now();
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 1000000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(1000000001, 2000000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(2000000001, 3000000000));
		uLong_ res4 = res1.get().cast_<uLong_>();
		uLong_ res5 = res2.get().cast_<uLong_>();
		uLong_ res6 = res3.get().cast_<uLong_>();
		res = res4 + res5 + res6;
		// 获取结束时间点
		auto stop = std::chrono::high_resolution_clock::now();
		// 计算所消耗的时间
		duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);	
		std::cout << "计算值为" << res << "多线程耗时：" << duration.count() << " milliseconds" << std::endl;
	}
	uLong_ sum_ = 0;	
	// 获取开始时间点
	auto start2 = std::chrono::high_resolution_clock::now();
	for (uLong_ i = 1; i <= 3000000000; i++)
	{
		sum_ += i;
	}
	// 获取结束时间点
	auto stop2 = std::chrono::high_resolution_clock::now();
	// 计算所消耗的时间
	auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(stop2 - start2);
	std::cout << "计算值为" << sum_ << "单线程耗时：" << duration2.count() << " milliseconds" << std::endl;
}
