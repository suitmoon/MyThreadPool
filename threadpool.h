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
#include <iostream>
#include <thread>


// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,  // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

// 任务抽象基类
class Result;
class Any;
class Task{

public:
	Task();
	~Task() = default;

	void exec();

	void setResult(Result* result);
	virtual Any run() = 0;
	
private:
	Result* result_;
};

class Thread{
public:
	//启动线程
	using ThreadFunc = std::function<void(int)>;
	//线程构造
	Thread(ThreadFunc func);
	//线程析构
	~Thread();
	//启动线程
	void start();
	// 获取线程id
	int getId()const;


private:
	ThreadFunc func_;
	int threadId_;  // 保存线程id
	static int generateId_;
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
	Result submitTask(std::shared_ptr<Task> sp);


	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&)= delete;


private:
	//定义线程函数
	void threadFunc(int threadid);

	// 检查pool的运行状态
	bool checkRunningState() const;


private:
	//std::vector<std::unique_ptr<Thread> > threads_; 
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表


	int initThreadSize_; // 初始的线程数量
	int threadSizeThreshHold_; // 线程数量上限阈值
	std::atomic_int curThreadSize_;	// 记录当前线程池里面线程的总数量
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量

	
	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_;  // 任务队列数量上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 等到线程资源全部回收

	PoolMode poolMode_; // 当前线程池的工作模式
	std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态
	};

class Any
{
public:
	Any() = default;
	~Any() = default;	
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;	
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//这个构造函数可以让Any类型接收任意其它的数据
	template <typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data)) {}


	// 这个方法能把Any对象里面存储的data数据提取出来
	template <typename T>
	T cast_()
	{
		Derive <T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw("type is unmatch!");
		}
		return pd->data_;
	}

private:
	// 基类类型
	class Base
	{
	public:
		
		virtual ~ Base() = default;
	};

	// 派生类类型
	template<typename T>
	class Derive:public Base
	{
	public:
		Derive (T data):data_(data)
		{}
		T data_;

	};
private:
	// 定义一个基类的指针
	std::unique_ptr<Base> base_;

};

//实现一个信号量类
class Semaphore {
public:
	Semaphore(int limit = 0) :resLimit_(limit) {}
	~Semaphore() = default;
	//获取一个信号量资源
	void wait();	
	//增加一个信号量资源
	void post();
	
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

//实现接收提交到线程池的task任务执行完成居的返回值类型Result
class Result {
	public:
		Result(std::shared_ptr<Task> task, bool isValid = true);
		~Result() = default;
		Any get();
		void setVal(Any any);
	private:
		Any any_; //存储任务的返回值
		Semaphore sem_; //线程通信信号量
		std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
		std::atomic_bool isValid_; // 返回值是否有效
};
#endif
