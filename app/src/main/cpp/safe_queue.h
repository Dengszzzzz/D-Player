#ifndef D_PLAYER_SAFE_QUEUE_H
#define D_PLAYER_SAFE_QUEUE_H

#include <queue>
#include <pthread.h>

using namespace std;

template<typename T>  // 泛型：存放任意类型
class SafeQueue{

private:
    //为复杂的声明定义一个简单的别名
    //ReleaseCallback是我们定义的别名，表示的是一个指向函数的指针，
    //该函数有一个泛型参数，传的是地址，返回void类型。
    //则ReleaseCallback类型的对象可以指向任何符合上述规则的函数
    typedef void (*ReleaseCallback)(T *);

private:
    queue<T> queue;  //定义队列
    pthread_mutex_t mutex; //定义互斥锁（不允许有野指针）
    pthread_cond_t cond;   //条件变量，为了实现 等待-唤醒功能（不允许有野指针）
    int work;    //标记队列是否工作
    ReleaseCallback releaseCallback;

public:
    SafeQueue() {
        //初始化互斥锁
        pthread_mutex_init(&mutex,0);
        //初始化条件变量
        pthread_cond_init(&cond,0);
    }

    ~SafeQueue(){
        //回收 互斥锁
        pthread_mutex_destroy(&mutex);
        //回收 条件变量
        pthread_cond_destroy(&cond);
    }

    /**
     * 入队 [ AVPacket *  压缩包]  [ AVFrame * 原始包]
     * */
    void insertToQueue(T value){
        //加锁
        pthread_mutex_lock(&mutex);

        if(work){
            //工作状态
            queue.push(value);
            pthread_cond_signal(&cond);  //当插入数据包进队列后，要发送通知唤醒。 -- Java notify
            //FixMe：为什么不用broadcast？ -- Java notifyAll
            //pthread_cond_broadcast(&cond);
        }else{
            //非工作装填，释放value，不知道如何释放，T类型不明确，所以要让外界释放
            if (releaseCallback){
                releaseCallback(&value); // 让外界释放我们的 value
            }
        }

        //解锁
        pthread_mutex_unlock(&mutex);
    }

    /**
     * 出队 [ AVPacket *  压缩包]  [ AVFrame * 原始包]
     * */
    int getQueueAndDel(T &value){
        int ret = 0;
        pthread_mutex_lock(&mutex);
        while(work && queue.empty()){
            //如果是工作中，并且队列里面没有数据，则阻塞等待 -- 相当于Java的 wait，注意，有可能被系统唤醒，所以放在while里
            pthread_cond_wait(&cond,&mutex);
        }

        if (!queue.empty()){
            value = queue.front();
            queue.pop();  //删除队列中的数据
            ret = 1;      //成功了 Success 返回值 true
        }

        pthread_mutex_unlock(&mutex);
        return ret;
    }

    /**
     * 设置工作状态
     * */
    void setWork(int work){
        pthread_mutex_lock(&mutex);
        this->work = work;
        //每次设置后，都去唤醒下，看有没阻塞睡觉的地方
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    int empty(){
        return queue.empty();
    }

    int size(){
        return queue.size();
    }

    /**
     * 清空队列中所有数据，循环一个一个地删除
     * */
    void clear(){
        pthread_mutex_lock(&mutex);
        unsigned int size = queue.size();
        for (int i = 0; i < size; ++i) {
            T value = queue.front();
            if (releaseCallback){
                releaseCallback(&value);  //让外界去释放堆区空间
            }
            queue.pop(); //删除队列中的数据，让队列为0
        }
        pthread_mutex_unlock(&mutex);
    }

    /**
     * 设置此函数指针的回调，让外界去释放
     * @param releaseCallback
     */
    void setReleaseCallback(ReleaseCallback releaseCallback){
        this->releaseCallback = releaseCallback;
    }
};

#endif //D_PLAYER_SAFE_QUEUE_H
