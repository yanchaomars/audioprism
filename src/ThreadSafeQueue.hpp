#ifndef _THREAD_SAFE_QUEUE_HPP
#define _THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

template <typename T>
class ThreadSafeQueue {
  public:
    void push(T value);
    T pop();

    unsigned int count();
    bool empty();

    template <typename Rep, typename Period>
    void wait(const std::chrono::duration<Rep, Period> &rel_time);

  private:
    std::mutex lock;
    std::condition_variable cvNotEmpty;
    std::queue<T> queue;
};

template <typename T>
unsigned int ThreadSafeQueue<T>::count() {
    std::lock_guard<std::mutex> lg(lock);
    return queue.size();
}

template <typename T>
bool ThreadSafeQueue<T>::empty() {
    std::lock_guard<std::mutex> lg(lock);
    return queue.empty();
}

template <typename T>
void ThreadSafeQueue<T>::push(T value) {
    std::lock_guard<std::mutex> lg(lock);
    queue.push(value);
    cvNotEmpty.notify_all();
}

template <typename T>
T ThreadSafeQueue<T>::pop() {
    std::unique_lock<std::mutex> lg(lock);
    if (queue.empty())
        cvNotEmpty.wait(lg);
    T value(std::move(queue.front()));
    queue.pop();
    return value;
}

template <typename T>
template <typename Rep, typename Period>
void ThreadSafeQueue<T>::wait(const std::chrono::duration<Rep, Period> &rel_time) {
    std::unique_lock<std::mutex> lg(lock);
    if (queue.empty())
        cvNotEmpty.wait_for(lg, rel_time);
}

#endif
