
/**
 * Created by jianxi on 2017/5/31.
 * https://github.com/mabeijianxi
 * mabeijianxi@gmail.com
 */
#ifndef EASYPLAYER_THREADSAFE_QUEUE_CPP
#define EASYPLAYER_THREADSAFE_QUEUE_CPP


#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
/**
 * 一个安全的队列
 */
class threadsafe_queue {
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<uint8_t *>> data_queue;
    std::condition_variable data_cond;
    std::condition_variable data_full;
    int max_size = -1;
    bool stop = false;
public:
    threadsafe_queue(int max){
        max_size = max;
        stop = false;
    }
    threadsafe_queue() {
        stop = false;
    }

    threadsafe_queue(threadsafe_queue const &other) {
        std::unique_lock<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }

    void push(uint8_t * new_value)//入队操作
    {
        std::unique_lock<std::mutex> lk(mut);
        if(max_size>0&&data_queue.size()>=max_size){
            data_full.wait(lk);
        }
        if(!stop){
            if(new_value){
                auto tmp = std::make_shared<uint8_t *>(new_value);
                data_queue.push(tmp);
            }else{
                data_queue.push(nullptr);
            }
        } else{
            free(new_value);
        }
        data_cond.notify_one();
    }

    std::shared_ptr<uint8_t *> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty()||stop; });
        auto result = data_queue.front();
        if(!stop){
            data_queue.pop();
            data_full.notify_one();
        }
        return result;
    }
    void wait_and_pop(uint8_t **value)//直到有元素可以删除为止
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] { return !data_queue.empty()||stop;; });
        if(stop){
            *value = NULL;
        } else
            value = data_queue.front().get();
        data_queue.pop();
    }
    bool empty() const {
        return data_queue.empty();
    }
    void set_max_size(int max){
        max_size = max;
    }
    void clear(){
        stop = true;
        data_cond.notify_all();
        std::unique_lock<std::mutex> lk(mut);
        while(!data_queue.empty()){
            auto tmp = data_queue.front();
            data_queue.pop();
            if(tmp){
                free(*tmp.get());
            }
        }
        data_full.notify_one();
    }
};

#endif //EASYPLAYER_THREADSAFE_QUEUE_CPP
