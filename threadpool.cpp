#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <string>
#include <future>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 3;//单位：秒

// 线程池的实现							
// 线程池构造
ThreadPool::ThreadPool() :
	initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
	, idleThreadSize_(0)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, curThreadSize_(0)
{}
// 线程池析构
ThreadPool::~ThreadPool()
{
	//置标记为为关闭
	isPoolRunning_ = false;

	//等待线程池所有线程返回
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}
// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}
void ThreadPool::turnoffpool() {
	isPoolRunning_ = false;
}
// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())
	{
		return;
	}
	taskQueMaxThreshHold_ = threshhold;
}
// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;
	}
}

// 开启线程池
void ThreadPool::start(int initThreadSize) {

	//设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int threadId_ = 0; threadId_ < initThreadSize_; threadId_++) {		
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1),threadId_);
		threadIds_.insert(threadId_);
		//int threadId = ptr->getId();		
		threads_.emplace(threadId_, std::move(ptr));
	}

	//启动所有线程
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start();
		idleThreadSize_++;   // 记录初始空闲线程的数量
	}
}

// 定义线程函数
void ThreadPool::threadFunc(int threadid) {

	auto lastTime = std::chrono::high_resolution_clock().now();
	while (isPoolRunning_) {
		//创建用以接受任务
		Task task;
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "线程尝试获取任务，id为" << std::this_thread::get_id() << std::endl;				

			while (taskQue_.size() == 0)
			{
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					//条件变量，超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{							
							threads_.erase(threadid);
							idleThreadSize_--;
							curThreadSize_--;
							//lock.unlock();
							//std::unique_lock<std::timed_mutex> threadlock(threadMtx_);
							threadIds_.erase(threadid);
							std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;

							return;
						}
					}
				}
				else {
					//等待notEmpty条件
					notEmpty_.wait(lock);
				}
				//线程池结束
				if (!isPoolRunning_)
				{
					threads_.erase(threadid);
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
					exitCond_.notify_all();
					return;
				}
			}
			std::cout << "线程获取任务成功，id为" << std::this_thread::get_id() << std::endl;
			idleThreadSize_--;
			//从任务队列种取一个任务出来。
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//如果依然有剩余任务，继续通知其它得线程执行任务
			if (taskSize_ > 0)
			{
				notEmpty_.notify_all();
			}

			//取出一个任务，进行通知，通知可以继续提交生产任务
			notFull_.notify_all();

		}
		//4、当前线程负责执行这个任务
		if (task != nullptr)
		{
			task();
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
	threads_.erase(threadid);
	std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
	exitCond_.notify_all();
	return;
}

// 检查pool的运行状态
bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}
//线程方法实现
//线程构造
Thread::Thread(ThreadFunc func,int threadId_) :func_(func
), threadId_(threadId_)
{};
//线程析构
Thread::~Thread() {};
//int Thread::generateId_ = 0;
// 启动线程
void Thread::start() {
	//创建一个线程来执行一个线程函数
	std::thread t(func_, threadId_);
	t.detach();//分离线程
}
int Thread::getId() const {	return threadId_;}

