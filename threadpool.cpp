#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <string>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 3; // 单位：秒


// 线程池的实现							
// 线程池构造
ThreadPool::ThreadPool() :
	initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED) 
	,isPoolRunning_(false)
	,idleThreadSize_(0)
	,threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	,curThreadSize_(0)
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

// 给线程池提交任务
//用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	if (!checkRunningState()) {
		std::cerr << "pool is close, submit task fail" << std::endl;
		return Result(sp, false);
	}
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//线程的通信等待任务队列有空余
	//用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
	if (!notFull_.wait_for(lock,std::chrono::seconds(1) ,
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; })) {
		std::cerr<< "task queue is full, submit task fail"<<std::endl;
		return Result(sp,false);
	}
	//如果有空余，把任务放入任务队列中
	taskQue_.emplace(sp);
	taskSize_++;
	//因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知，赶快分配线程执行任务
	notEmpty_.notify_all();
	//cached模式
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// 创建新的线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// 启动线程
		threads_[threadId]->start();
		// 修改线程个数相关的变量
		curThreadSize_++;
		idleThreadSize_++;
	}
	
	//返回任务的Result对象
	return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize){
	
	//设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));		
		int threadId = ptr->getId();
		threads_.emplace(threadId,std::move(ptr));
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
	while(isPoolRunning_) {
		
		//创建用以接受任务
		std::shared_ptr<Task> task;		
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
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
		if (taskSize_>0)
		{
			notEmpty_.notify_all();
		}
		
		//取出一个任务，进行通知，通知可以继续提交生产任务
		notFull_.notify_all();

		}
		//4、当前线程负责执行这个任务
		if (task!=nullptr)
		{
			task->exec();
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
bool ThreadPool::checkRunningState() const{
	return isPoolRunning_;
}


//线程方法实现
//线程构造
Thread::Thread(ThreadFunc func):func_(func
), threadId_(generateId_++)
{};
//线程析构
Thread::~Thread() {};

int Thread::generateId_=0;
// 启动线程
void Thread::start() {
	//创建一个线程来执行一个线程函数
	std::thread t(func_,threadId_);
	t.detach();//分离线程
}

int Thread::getId() const {
	return threadId_;
}


//信号量的实现
void Semaphore::wait(){
	std::unique_lock<std::mutex> lock(mtx_);
	cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
	resLimit_--;
}

void Semaphore::post() {
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}


//Result方法的实现
Result::Result(std::shared_ptr<Task> task, bool isValid) : task_(task), isValid_(isValid) {
	task_->setResult(this);
}

Any Result::get() {
	if (!isValid_) { return ""; };	
	sem_.wait();//task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any) {
	//存储task的返回值
	this->any_ = std::move(any);
	sem_.post();//已经获取的任务的返回值，增加信号量资源
}

//Task方法的实现
Task::Task():result_(nullptr) 
{}

void Task::exec() {
	if (result_!=nullptr)
	{
		result_->setVal(run());
	}	
}

void Task::setResult(Result* result) {
	result_ = result;
}