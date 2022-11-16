#include "ux_server.h"

ux_server::ux_server()
{
    //===== 添加任务函数到容器 =====
    //账号注册
    map_func.insert(pair<enum_transmit,function<int(int)>>
             (e_register,bind(&ux_server::task_register,this,_1)));

    //登录请求
    map_func.insert(pair<enum_transmit,function<int(int)>>
             (e_login,bind(&ux_server::task_login,this,_1)));

    //账号退出
    map_func.insert(pair<enum_transmit,function<int(int)>>
             (e_logout,bind(&ux_server::task_logout,this,_1)));



    //===== 添加任务函数到容器 =====

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
            function<int(int)> func = it_find->second;
            size_call = func(fd);

            if(size_call > 0) vlog("分发任务:处理结束--执行成功");
            else vlog("分发任务:处理结束--执行失败");
        }
        else
        {
            //发生错误，清空缓冲区
            vlog("未找到对应任务函数");
            char buf[1024];
            while(read(fd,buf,sizeof(buf)) > 0){}
        }
    }
    else vlog("读取失败--消息头");

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
        ct_back.account = 123456789;//申请并发放账号  ==============================
        strncpy(ct_back.passwd,ct.passwd,sizeof(ct_back.passwd));

        //反馈到连接者
        ssize_t back = 0;
        back += write(fd,(char*)&cmd,sizeof(cmd));
        back += write(fd,(char*)&ct_back,sizeof(ct_back));

        if(back != (sizeof(cmd) + sizeof(ct_back)))
        {
            //发送失败...
            vlog("发送反馈--失败");
        }
        else vlog("发送反馈--成功");
    }
    else vlog("读取失败--消息内容");

    return size;
}

int ux_server::task_login(int fd)
{
    //读取内容到协议结构体
    ct_login ct;
    ssize_t size = read(fd,(char*)&ct,sizeof(ct));

    if(size > 0)
    {
        vlog("登录请求，验证账号密码: |=%lld=|=%s=|",ct.account,ct.passwd);

        //返回消息初始化
        ct_login_back ct_back;
        char back_buf[sizeof(ct_back.info)];
        memset(back_buf,0,sizeof(back_buf));
        strncpy(back_buf,"登录失败，账号或者密码不正确",sizeof(back_buf));
        int back_flg = 0;

        //账号验证  ==============================
        if(ct.account == 123123 && (strcmp(ct.passwd,"123123") == 0))
        {
            vlog("验证成功");
            back_flg = 1;
            strncpy(back_buf,"登录成功",sizeof(back_buf));
        }
        else vlog("验证失败");

        //发送给连接者
        ct_cmd cmd;
        cmd.type = enum_transmit::e_login_back;

        //反馈到连接者
        ssize_t back = 0;
        ct_back.flg = back_flg;
        strncpy(ct_back.info,back_buf,sizeof(ct_back.info));
        back += write(fd,(char*)&cmd,sizeof(cmd));
        back += write(fd,(char*)&ct_back,sizeof(ct_back));

        if(back != (sizeof(cmd) + sizeof(ct_back)))
        {
            //发送失败...
            vlog("发送反馈--失败");
        }
        else vlog("发送反馈--成功");

    }
    else vlog("读取失败--消息内容");

    return size;
}

int ux_server::task_logout(int fd)
{
    //读取内容到协议结构体
    ct_logout ct;
    ssize_t size = read(fd,(char*)&ct,sizeof(ct));

    if(size > 0)
    {
        vlog("账号退出");

        //===========================


    }
    else vlog("读取失败--消息内容");

    return size;
}

