#include "threadpool.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>



using uLong_ = unsigned long long;
uLong_ _sum(uLong_ a, uLong_ b)
{
	uLong_ sum_ = 0;
	for (uLong_ i = a; i <= b; i++)
	{
		sum_ += i;
	}
	return sum_;
}


int main()
{	
	uLong_ res = 0;
	std::chrono::milliseconds duration;
	
	{
	ThreadPool pool;
	pool.setMode(PoolMode::MODE_CACHED);
	pool.start();
	std::future<uLong_> r1 = pool.submitTask(_sum, 1, 1000000000);
	auto start = std::chrono::high_resolution_clock::now();
	std::future<uLong_> r2 = pool.submitTask(_sum, 1000000001, 2000000000);
	std::future<uLong_> r3 = pool.submitTask(_sum, 2000000001, 3000000000);

	/*std::future<uLong_> r4 = pool.submitTask(sum1, 2000000001, 3000000000);	
	std::future<uLong_> r53 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r531 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r532 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r533 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r153 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r1531 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r1532 = pool.submitTask(sum1, 2000000001, 3000000000);
	std::future<uLong_> r1533 = pool.submitTask(sum1, 2000000001, 3000000000);
	*/



	uLong_ res1 = r1.get();
	uLong_ res2 = r2.get();
	uLong_ res3 = r3.get();
	res = res1 + res2 + res3;	
	// 获取结束时间点
	auto stop = std::chrono::high_resolution_clock::now();
	// 计算所消耗的时间
	duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	
	}
	std::cout << "计算值为" << res << "多线程耗时：" << duration.count() << " milliseconds" << std::endl;

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