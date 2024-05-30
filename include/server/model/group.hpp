#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>
using namespace std;

//Group表的ORM类
class Group
{
public:
    //构造函数
    Group(int id=-1,string name="",string desc=""){
        this->id=id;
        this->name=name;
        this->desc=desc;
    }
    //成员函数
    //设置变量值
    void setId(int id){this->id=id;} 
    void setName(string name){this->name=name;}
    void setDesc(string desc){this->desc=desc;}

    //获取变量值
    int getId(){return this->id;}
    string getName(){return this->name;}
    string getDesc(){return this->desc;}
    vector<GroupUser>& getUsers(){return this->users;} //获取群组中都有哪些用户，返回的是一个引用

private:
    int id;      //群组id
    string name; //群组名称
    string desc; //群组的描述信息
    vector<GroupUser> users; //加入该群组的用户
};

#endif