/*************************************************************************
	> File Name: logrec.cpp
	> Author: 
	> Mail: 
	> Created Time: Fri 13 Oct 2017 04:19:39 PM CST
 ************************************************************************/
 #include <fstream>
 #include <ace/Synch.h>
 #include <ace/Task.h>
 #include <ace/Message_Block.h>
 #include <ace/Stream.h>
 #include "ace/Thread_Manager.h"
 #include <ace/Time_Value.h>
 #include <ace/Module.h>

using namespace std;

class Logrec_Reader : public ACE_Task<ACE_MT_SYNCH>
{
private:
    ifstream fin;  //标准输入流
public:
    Logrec_Reader(ACE_TString logfile)
    {
        fin.open(logfile.c_str()); //ACE_TString.c_str() 转换为char
    }
    virtual int open (void *)
    {
        return activate();
    }

    virtual int svc()
    {
        ACE_Message_Block *mblk;
        int len = 0;
        const int LineSize = 256;
        char file_buf[LineSize];

        while(!fin.eof())
        {
            fin.getline(file_buf, LineSize);
            len = ACE_OS::strlen(file_buf);
            ACE_NEW_RETURN(mblk, ACE_Message_Block(len + 200), 0);
            if (file_buf[len - 1] == '\r')
            {
                len = len - 1;
            }
            mblk->copy(file_buf, len);
            // 通过put_next函数，将消息传递给下一个过滤器
            put_next(mblk);
        }
        //构造一个MB_STOP消息
        ACE_NEW_RETURN(mblk, ACE_Message_Block (0, ACE_Message_Block::MB_STOP), 0);
        put_next(mblk);
        fin.close();
        ACE_DEBUG((LM_DEBUG, "read svc return. \n"));
        return 0;
    }
};

class Logrec_Timer : public ACE_Task<ACE_SYNCH>
{
private:
    void format_data(ACE_Message_Block *mblk)
    {
        char *msg = mblk->data_block()->base();
        strcat(msg, "format_data");
    }
public:
    virtual int put(ACE_Message_Block *mblk, ACE_Time_Value *)
    {
        for (ACE_Message_Block *temp = mblk;
            temp != 0; temp = temp->cont())
        {
            if (temp->msg_type() != ACE_Message_Block::MB_STOP)
            {
                format_data(temp);
            }
        }
        return put_next(mblk);
    }
};

class Logrec_Suffix : public ACE_Task<ACE_SYNCH>
{
public:
    void suffix(ACE_Message_Block *mblk)
    {
        char *msg = mblk->data_block()->base();
        strcat(msg, "suffix\n");
    }
    virtual int put(ACE_Message_Block *mblk, ACE_Time_Value *)
    {
        for (ACE_Message_Block *temp = mblk;
            temp != 0; temp = temp->cont())
        {
            if (temp->msg_type() != ACE_Message_Block::MB_STOP)
            {
                suffix(temp);
            }
        }
        return put_next(mblk);
    }
};

class Logrec_Write : public ACE_Task<ACE_SYNCH>
{
public:
    virtual int open(void*)
    {
        ACE_DEBUG((LM_DEBUG, "Logrec_Writer.\n"));
        return activate();
    }

    virtual int put (ACE_Message_Block *mblk, ACE_Time_Value *to)
    {
        return putq(mblk, to);
    }

    virtual int svc()
    {
        int stop = 0;
        for (ACE_Message_Block *mb; !stop && getq(mb) != -1;)
        {
            if (mb->msg_type() == ACE_Message_Block::MB_STOP)
            {
                stop = 1;
            }
            else{
                ACE_DEBUG((LM_DEBUG, "%s", mb->base()));
            }
            put_next(mb);
        }
        return 0;
    }
};

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
    if (argc != 2)
    {
        ACE_ERROR_RETURN((LM_ERROR, "usage: %s logfile\n", argv[0]), 1);
    }

    ACE_TString logfile (argv[1]);

    ACE_Stream<ACE_SYNCH> stream;


    ACE_Module<ACE_MT_SYNCH> *module[4];
    module[0] = new ACE_Module<ACE_MT_SYNCH>("Reader", new Logrec_Reader(logfile), 0, 0, ACE_Module<ACE_SYNCH>::M_DELETE_READER);
    module[1] = new ACE_Module<ACE_MT_SYNCH>("Formatter", new Logrec_Timer(), 0, 0, ACE_Module<ACE_SYNCH>::M_DELETE_READER);
    module[2] = new ACE_Module<ACE_MT_SYNCH>("Separator", new Logrec_Suffix(), 0, 0, ACE_Module<ACE_SYNCH>::M_DELETE_READER);
    module[3] = new ACE_Module<ACE_MT_SYNCH>("Writer", new Logrec_Write(), 0, 0, ACE_Module<ACE_SYNCH>::M_DELETE_READER);

    for ( int i = 3; i >= 0; --i )
    {
        if (stream.push(module[i]) == -1)
        {
            ACE_ERROR_RETURN((LM_ERROR, "push %s module into stream failed.\n", module[i]->name()), 1);
        }
        ACE_DEBUG((LM_DEBUG, "push %s module into stream success. \n", module[i]->name()));
    }
    ACE_Thread_Manager::instance()->wait();
}
