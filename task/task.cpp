/*************************************************************************
	> File Name: task.cpp
	> Author: 
	> Mail: 
	> Created Time: Tue 10 Oct 2017 02:49:52 PM CST
 ************************************************************************/

#include <ace/Synch.h>
#include <ace/Task.h>
#include <ace/Message_Block.h>

char test_message[] = "test_message";
#define MAX_MESSAGES 10
class Counting_Test_Producer : public ACE_Task<ACE_MT_SYNCH>
{
    public:
        Counting_Test_Producer (ACE_Message_Queue<ACE_MT_SYNCH> *queue)
        :ACE_Task<ACE_MT_SYNCH>(0,queue) {}
        virtual int svc (void);
};

int Counting_Test_Producer::svc (void)
{
    int produced = 0;
    char data[256] = {0};
    ACE_Message_Block * b = 0;

    while(1)
    {
        ACE_OS::sprintf(data, "%s--%d.\n", test_message, produced);

        //创建消息块
        ACE_NEW_NORETURN (b, ACE_Message_Block (256));
        if (b == 0)
        {
            break;
        }
        
        //将data中的数据复制到消息块中
        b->copy(data, 256);
        if (produced >= MAX_MESSAGES)
        {
            //如果是最后一个数据，那么将数据属性设置为MB_STOP
            b->msg_type(ACE_Message_Block::MB_STOP);

            //将消息块放入队列中
            if (this->putq(b) == -1)
            {
                b->release();
                break;
            }
            produced ++;
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Producer put the data: %s.\n"), b->base()));
            break;
        }
        if (this->putq(b) == -1)
        {
            b->release();
            break;
        }
        produced ++;

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Producer put the data: %s.\n"), b->base()));
        ACE_OS::sleep(1);
    }
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Producer done\n")));
    return 0;
}

class Counting_Test_Consumer : public ACE_Task<ACE_MT_SYNCH>
{
    public:
        Counting_Test_Consumer (ACE_Message_Queue<ACE_MT_SYNCH> *queue)
        :ACE_Task<ACE_MT_SYNCH> (0, queue){}
        virtual int svc(void);
};

int Counting_Test_Consumer::svc(void)
{
    int consumer = 0;
    ACE_Message_Block *b = 0;
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT("in consumer svc.\n")));
    ACE_OS::sleep(30);
    while(1)
    {
        //循环从队列中读取数据块，如果读取失败，那么退出线程
        if (this->getq(b) == -1)
        {
            break;
        }
        if (b->msg_type() == ACE_Message_Block::MB_STOP)
        {
            //如果消息属性是MB_STOP，那么表示其为最后一个数据
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Consumer get the data: %s.\n"), b->base()));
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Consumer get the stop msg.\n")));
            b->release();
            consumer++;
            break;
        }
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Consumer get the data: %s.\n"), b->base()));
        b->release();
        consumer++;
        ACE_OS::sleep(5);
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Consumer done\n")));
    return 0;
}

int ACE_MAIN(int argc, ACE_TCHAR *argv[])
{
    //创建消息队列
    ACE_Message_Queue<ACE_MT_SYNCH> queue(2*1024*1024);

    // 创建生产者和消费者，它们使用同一个消息队列，只有这样才能实现线程间消息的传递
    Counting_Test_Producer producer(&queue);
    Counting_Test_Consumer consumer(&queue);

    //调用activate函数创建消费者线程
    if (consumer.activate(THR_NEW_LWP | THR_DETACHED | THR_INHERIT_SCHED, 1) == -1)
    {
        ACE_ERROR_RETURN ((LM_ERROR, ACE_TEXT("Consumers %p\n"), ACE_TEXT("activate")), -1);
    }

    //调用activate函数创建生产者线程
    if (producer.activate ( THR_NEW_LWP | THR_DETACHED | THR_INHERIT_SCHED, 1) == -1)
    {
        ACE_ERROR((LM_ERROR, ACE_TEXT("Producers %p\n"), ACE_TEXT("activate")));
        consumer.wait();
        return -1;
    }
    //调用wait函数等待线程结束
    ACE_Thread_Manager::instance()->wait();
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Ending test!\n")));
    return 0;
}
