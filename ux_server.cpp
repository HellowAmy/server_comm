#include "ux_server.h"

ux_server::ux_server()
{
    //添加任务函数到容器
    map_func.insert(pair<enum_transmit,function<int(int)>>
             (e_register,bind(&ux_server::task_register,this,_1)));

}

void ux_server::sock_new(int fd,sockaddr_in sock)
{
    vlog("sock_new  %d",fd);
}

void ux_server::sock_close(int fd)
{
    vlog("sock_close  %d",fd);
}

int ux_server::sock_read(int fd)
{
    //读取操作协议
    ct_cmd cmd;
    ssize_t size = read(fd,(char*)&cmd,sizeof(cmd));
    ssize_t size_call = 0;

    if(size > 0)
    {
        //从map中查找匹配函数
        auto it_find = map_func.find((enum_transmit)cmd.type);

        //执行匹配的任务函数
        if(it_find != map_func.end())
        {
            vlog("进入任务");
            function<int(int)> func = it_find->second;
            size_call = func(fd);
        }
        else vlog("未找到对应任务函数");

        if(size > 0 && size_call > 0) vlog("任务函数执行成功");
    }

    size += size_call;
    return size;
}

int ux_server::task_register(int fd)
{
    //读取内容到协议结构体
    ct_register ct;
    ssize_t size = read(fd,(char*)&ct,sizeof(ct));

    if(size > 0)
    {
        vlog("即将返回申请账号，当前密码：%s",ct.passwd);

        //发送给连接者
        ct_cmd cmd;
        cmd.type = enum_transmit::e_register_back;
        ct_register_back ct_back;
        ct_back.account = 123456789;//申请并发放账号
        strncpy(ct_back.passwd,ct.passwd,sizeof(ct_back.passwd));

        ssize_t back = 0;
        back += write(fd,(char*)&cmd,sizeof(cmd));
        back += write(fd,(char*)&ct_back,sizeof(ct_back));

        if(back < 0)
        {
            //发送失败...
            vlog("发送失败");
        }
    }

    return size;
}

