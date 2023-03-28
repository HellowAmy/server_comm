#ifndef QT_SHOW_H
#define QT_SHOW_H

//===== 日志宏 =====
#define vv(value) "["#value": "<<value<<"] "
#define vloge(...) std::cout<<"\033[31m[Err] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogw(...) std::cout<<"\033[33m[War] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogd(...) std::cout<<"\033[32m[Deb] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogf(...) std::cout<<"[Inf] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<endl
//===== 日志宏 =====


//#include "log_show.h"


//#define OPEN_QT
#define OPEN_CPP


//===== QT =====
#ifdef OPEN_QT


#include <QDebug>

#ifndef out
#define out qDebug()
#endif

//=====
//模板打印--字符
template<class T>
void show(const T &buf)
{
    out<<buf;
}

//模板打印--数组
template<class T>
void show_arr(const T vec,QString flg = "|")
{
    out<<"===== " +flg+ " =====";
    for(int i = 0;i<vec.size();i++)
    {
        out<<vec[i];
    }
    out<<"====================";
}
//=====


#endif // OPEN_QT
//===== QT =====






//===== C++ =====
#ifdef OPEN_CPP


//=====
//带换行日志
#define vlog(format, ...) \
    do {fprintf(stderr, "( %s | %s:%d ) ========== { " format " }\n", \
     __FILE__, __func__, __LINE__, ##__VA_ARGS__);} while (0)

//不换行日志
#define clog(format, ...) \
    do {fprintf(stderr, "( %s | %s:%d ) " format , \
     __FILE__, __func__, __LINE__, ##__VA_ARGS__);} while (0)
//=====


#endif // OPEN_CPP
//===== C++ =====


#endif // QT_SHOW_H
