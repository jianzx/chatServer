#include "friendmodel.hpp"
#include "db.h"

//添加好友信息
void FriendModel::insert(int userid,int friendid)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"insert into friend values(%d,%d)",userid,friendid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//返回用户好友列表
vector<User> FriendModel::query(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    //对user表和friend表进行联合查询
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid=a.id where b.userid=%d", userid);

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有好友信息放入vec中返回
            MYSQL_ROW row; //循环读取，一个用户可能有多个好友
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec; //如果没有好友返回的是空向量
}