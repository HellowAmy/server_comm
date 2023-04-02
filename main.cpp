#include "ux_epoll.h"
#include <cstring>

//===== 结构体转换string函数 =====
//结构体转string
//      语法解析：(char*)&ct ，由&ct获取结构体地址，在由该地址(char*)转为char*类型的指针
//      根据string构造函数，参数1：char*地址，参数2：长度，可从地址内存中复制二进制内容
template <class T_ct>
static string ct_s(T_ct ct)
{ return string((char*)&ct,sizeof(T_ct)); }

//string转结构体
//      语法解析：*(T_ct*)str.c_str() ，由str.c_str()从string类获取const char*指针，
//      由const char*指针转为T_ct*指针，再*（T_ct*）从指针中获取值，从而返回值
template <class T_ct>
static T_ct st_c(const string &str)
{ T_ct ct = *(T_ct*)str.c_str(); return ct; }
//===== 结构体转换string函数 =====


//===== 消息处理结构体 =====
//== 请求类型 ==
enum en_transmit
{
    e_login,    //登陆
    e_swap,     //交换
    e_notify,   //通知
};

//== 消息权限 ==
enum en_msg
{
    e_public,   //群发
    e_private,  //私聊
};

//== 服务器解析聊天消息交互协议 ==
struct ct_msg_swap
{
    en_transmit et;     //用于判断是登陆还是发送信息
    en_msg em;          //用于判断是私聊还是群发
    size_t number_to;   //用于判断私聊时的目标ID
    size_t number_from; //用于存储发送者ID
    char name[64];      //发送者的昵称
    char buf[2048];     //存放发送的内容(登陆时:昵称,转发时:信息)
};

//== 登陆信息记录 ==
struct ct_login_id
{
    int fd;                     //存放fd,用于转发信息时建立连接
    string name;                //存放连接用户的昵称
    shared_ptr<channel> pch;    //用于发送信息的接口
};
//===== 消息处理结构体 =====



int main()
{
    mutex mutex_read;                   //读锁
    map<size_t,ct_login_id> map_login;  //登陆的用户索引,用于查找
    size_t count_login = 0;             //分配ID
    ux_epoll server_epoll;              //epoll服务器

    //===== 回调区 =====
    //新连接
    server_epoll.sock_new = [&](const shared_ptr<channel> &pch,const string &ip){
        vlogd("sock_new: " vv(pch->get_fd()) vv(ip));
    };

    //读数据--服务器接收到数据
    server_epoll.sock_read = [&](const shared_ptr<channel> &pch,const string &msg){
        unique_lock<mutex> lock(mutex_read); //加锁是因为服务器读数据是多线程读取
        ct_msg_swap ct = st_c<ct_msg_swap>(msg); //字符串转结构体,无需引入json即可结构化数据

        //登陆的处理:分配ID,存储fd和昵称,反馈登陆ID,群发登陆用户信息
        if(ct.et == e_login)
        {
            //分配ID
            count_login++;

            //存储fd和昵称
            ct_login_id ct_id;
            ct_id.fd = pch->get_fd();
            ct_id.name = ct.buf;
            ct_id.pch = pch;
            map_login.insert(pair<size_t,ct_login_id>(count_login,ct_id));

            //反馈登陆ID
            ct.et = e_login;
            vlogd("count_login:" vv(to_string(count_login)));
            strncpy(ct.buf,to_string(count_login).c_str(),sizeof(ct.buf));
            pch->send_msg(ct_s(ct)); //结构体转string,并原路发送到客户端

            //群发登陆用户信息
            string content;
            ct.et = e_notify;
            content = "[登陆] [ID:" + to_string(count_login) +"] [昵称:"+ct_id.name+"]";
            strncpy(ct.buf,content.c_str(),sizeof(ct.buf));
            vlogd("群发:" vv(content) vv(ct_id.name) vv(count_login));
            for(const auto &a:map_login)
            { a.second.pch->send_msg(ct_s(ct)); }
        }

        //转发消息的处理:区分群发和私发,获取转发数据接口,转发数据
        else if(ct.et == e_swap)
        {
            if(ct.em == e_public)//群发
            {
                for(const auto &a:map_login)
                { a.second.pch->send_msg(ct_s(ct)); }
            }
            else if(ct.em == e_private)//私发
            {
                //查找并发送
                auto it = map_login.find(ct.number_to);
                if(it != map_login.end())
                    { it->second.pch->send_msg(ct_s(ct)); }
                else
                {
                    strncpy(ct.buf,"信息无法送达--请检查是否合理",sizeof(ct.buf));
                    ct.et = e_notify;
                    pch->send_msg(ct_s(ct));
                    vlogw("== number inexistence ==");
                }
            }
            else { vlogw("== en_msg inexistence =="); }
        }

        //所有用户信息请求:排队ID,通过ID获取昵称,原路反馈信息
        else if(ct.et == e_notify)
        {
            //排队ID
            vector<size_t> vec;
            for(const auto &a:map_login)
            { vec.push_back(a.first); }
            std::sort(vec.begin(),vec.end());

            //通过ID获取昵称
            string content = "\n";
            for(const auto &a:vec)
            {
                //查找并发送
                auto it = map_login.find(a);
                if(it != map_login.end())
                { content += to_string(it->first) + ":" + it->second.name + "\n"; }
            }

            //原路反馈信息
            ct.et = e_notify;
            strncpy(ct.buf,content.c_str(),sizeof(ct.buf));
            pch->send_msg(ct_s(ct));
        }
        else { vlogw("== en_transmit inexistence =="); }
    };

    //关闭连接--客户端主动关闭
    server_epoll.sock_close = [&](const shared_ptr<channel> &pch){
        unique_lock<mutex> lock(mutex_read);

        //群发通知信息有用户退出
        ct_msg_swap ct;
        memset(&ct,0,sizeof(ct));

        ct.et = e_notify;//通知类型
        for(auto a:map_login)
        {
            if(a.second.fd == pch->get_fd())
            {
                //记录退出信息
                string content = "[退出] [ID: "+to_string(a.first)+"] [name: "+a.second.name+"]";
                strncpy(ct.buf,content.c_str(),sizeof(ct.buf));
                map_login.erase(a.first);
                break;
            }
        }

        //群发通知
        for(const auto &a:map_login)
        { a.second.pch->send_msg(ct_s(ct));  }

        vlogd("channel 断开的fd :" vv(pch->get_fd()));
    };
    //===== 回调区 =====

    int ret = server_epoll.open_epoll(5005);    //启动服务器
    vlogd("open_epoll ret: " vv(ret));          //服务器退出

    printf("\n===== end =====\n");
    return 0;
}
