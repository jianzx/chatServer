#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

//维护群组信息的操作接口方法
class GroupModel
{
public:
    //创建群组
    bool createGroup(Group &group);
    //加入群组    （用户id,用户要加入的群组id，用户在群组中的角色）
    void addGroup(int userid,int groupid,string role);
    //查询用户所在群组信息，一个用户可能在多个群
    vector<Group> queryGroups(int userid);
    //根据指定的groupid查询群组用户id列表，除userid自己，主要用于群聊业务给群组其他成员群发消息
    vector<int> queryGroupUsers(int userid,int groupid);
};

#endif