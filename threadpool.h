#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <vector>
#include <memory>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <future>
#include <iostream>
#include <unordered_set>

// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,  // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

class Thread{

public:
	//启动线程
	using ThreadFunc = std::function<void(int)>;
	//线程构造
	Thread(ThreadFunc func, int threadId_);
	//线程析构
	~Thread();
	//启动线程
	void start();
	// 获取线程id
	int getId()const;


private:
	ThreadFunc func_;
	int threadId_;  // 保存线程id
	//static int generateId_;
};

class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	// 设置线程池的工作模式
	void setMode(PoolMode mode);

	// 设置task任务队列上线阈值
	void setTaskQueMaxThreshHold(int threshhold);

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold);

	// 给线程池提交任务
	template<typename Func,typename... Args>
	auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))> {
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		if (!checkRunningState()) {
			std::cerr << "pool is close, submit task fail" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()-> RType { return  RType(); });
			(*task)();
			return task->get_future();

		}
		//获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//线程的通信等待任务队列有空余
		//用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; })) {
			std::cerr << "task queue is full, submit task fail" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		std::cerr << "task queue submit task ok" << std::endl;
		//因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知，赶快分配线程执行任务
		notEmpty_.notify_all();

		//释放锁
//		lock.unlock();

		//如果是cache模式则抢线程锁创建新线程
//		std::unique_lock<std::timed_mutex> threadlock(threadMtx_);
//		if (!threadlock.try_lock_for(std::chrono::seconds(1))) {
			// 如果在1秒后仍无法获取锁，返回任务的Result对象					
//			return result;
//		}
		//cached模式
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			std::cout << ">>> create new thread..." << std::endl;						
			for (int threadId=initThreadSize_;; threadId++) {
				if (threadIds_.find(threadId) == threadIds_.end()) {
						// initThreadSize_不在threadIds_中
						threadIds_.insert(threadId);
						auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1), threadId);
						threads_.emplace(threadId, std::move(ptr));
						// 启动线程
						threads_[threadId]->start();
						// 修改线程个数相关的变量
						curThreadSize_++;
						idleThreadSize_++;
						break;
					}
			}						
		}
		//返回任务的Result对象
		return result;
	}


	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	//关闭线程池
	void turnoffpool();

private:
	//定义线程函数
	void threadFunc(int threadid);

	// 检查pool的运行状态
	bool checkRunningState() const;


private:
	//std::vector<std::unique_ptr<Thread> > threads_; 
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_set<int> threadIds_;


	int initThreadSize_; // 初始的线程数量
	int threadSizeThreshHold_; // 线程数量上限阈值
	std::atomic_int curThreadSize_;	// 记录当前线程池里面线程的总数量
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量

	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_;  // 任务队列数量上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
//	std::timed_mutex threadMtx_; // 保证线程合集的线程安全

	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 等到线程资源全部回收

	PoolMode poolMode_; // 当前线程池的工作模式
	std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态
};

#endif
