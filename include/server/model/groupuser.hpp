#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

//群组用户，多了一个role角色信息，从User类直接继承，复用User的其他信息
class GroupUser:public User //继承方式为公有继承
{
public:
    void setRole(string role){this->role=role;}
    string getRole(){return this->role;} 
private:
    string role;//用户在群组里的角色，分为creator和normal
};

#endif