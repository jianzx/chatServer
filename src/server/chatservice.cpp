#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
using namespace muduo;
using namespace std;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

//构造函数，注册消息及对应的Handler回调操作
ChatService::ChatService()
{
    //_msgHandlerMap存储消息id和其对应业务处理方法
    //注册消息
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    //登录消息
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    //注销消息
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});
    //添加好友消息
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    //一对一聊天消息
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    //创建群组消息
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    //加入群组消息
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    //群聊消息
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    string name=js["name"];
    string pwd=js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state=_userModel.insert(user); //在数据库中是否插入成功
    if(state){
        //注册成功,给用户返回一些信息
        json response;
        response["msgid"]=REG_MSG_ACK; //注册响应消息
        response["errno"]=0;           //0表示没有出错
        response["id"]=user.getId();
        conn->send(response.dump());
    }
    else{
        //注册失败
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;           //1表示出错
        conn->send(response.dump());
    }
}

//处理登录业务，验证密码是否和数据库中保存的用户密码一致
void ChatService::login(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int id=js["id"].get<int>();//用户用id登录
    string pwd=js["password"];//用户登录时提供的密码
    User user=_userModel.query(id);
    if(user.getId()==id && user.getPwd()==pwd){
        //该用户已经登录，不允许重复登录
        if (user.getState() == "online")
        {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;    //2表示登录出错
            response["errmsg"] = "this account is using,input another!";
            conn->send(response.dump());
        }
        else
        {
            {//登录成功，记录用户连接信息
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }//出括号，解锁

            //id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功，更新用户的状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            //登录成功后，查询当前用户是否有离线消息，有的话推给他
            vector<string> vec=_offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"]=vec;
                //读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            //登录成功后，查询该用户的好友信息，有的话显示
            vector<User> userVec=_friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user:userVec){
                    json js;   //把好友信息写成json字符串的格式再放进数组里
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }

            //登录成功后，查询用户的群组信息（包括群组成员的信息）
            vector<Group> groupVec=_groupModel.queryGroups(id);
            if(!groupVec.empty()){
                vector<string> groupV;
                for(Group &group:groupVec){
                    json grpjson;
                    grpjson["id"]=group.getId();
                    grpjson["groupname"]=group.getName();
                    grpjson["groupdesc"]=group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user:group.getUsers()){
                        json js;
                        js["id"]=user.getId();
                        js["name"]=user.getName();
                        js["state"]=user.getState();
                        js["role"]=user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"]=userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"]=groupV;
            }

            conn->send(response.dump());
        }
    }
    else{
        //登录失败
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=2;
        response["errmsg"]="id or password is invalid!";
        conn->send(response.dump());
    }
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            _userConnMap.erase(it);
        } 
    }

    //用户注销，相当于就是下线，在redis中取消订阅
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid,"","","offline");
    _userModel.updateState(user);

}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int toid=js["toid"].get<int>(); //收件人的id
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end()){
            //toid在线，转发消息,服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    //若是查不到有两种情况，一是对方和自己不在一台服务器上，二是对方不在线
    //查询toid是否在线
    User user=_userModel.query(toid);
    //第一种情况，在别的服务器上，通过redis消息队列发送消息
    if(user.getState()=="online"){ 
        _redis.publish(toid,js.dump());
        return;
    }

    //第二种情况，toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid,friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    string name=js["groupname"];
    string desc=js["groupdesc"];
    //存储新创建的群组信息
    Group group(-1,name,desc);//还没有添加到数据库，群的id是未知的；创建成功后会自动生成id
    if(_groupModel.createGroup(group)){ //创建群组成功
        //存储群组创建人信息,也就是创建的人加入该群，角色为creator
        _groupModel.addGroup(userid,group.getId(),"creator"); //在groupuser表中
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal"); //其他人加入群组是普通群成员
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);//因为涉及连接表的操作需要确保线程安全
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id); //查找这些群成员是否在线
        if (it != _userConnMap.end())
        {
            //群成员在线，转发群消息
            it->second->send(js.dump());
        }
        else
        {
            //查询群成员状态
            User user = _userModel.query(id);
            //在别的服务器上，通过redis消息队列发送消息
            if (user.getState() == "online")
            { 
                _redis.publish(id, js.dump());
            }
            else
            {
                //群成员不在线，存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        // 在连接表中查找该用户连接，删去，并将用户状态改为offline
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表中删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于就是下线，在redis中取消订阅
    _redis.unsubscribe(user.getId());

    if (user.getId() != -1) //确实是一个有效的用户
    {
        // 更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user); 
    }
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    //把online状态的用户设置成offline
    _userModel.resetState(); //操作数据库中的表
}

//从redis消息队列获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it=_userConnMap.find(userid);  //到当前用户所在的主机上来查找连接
    if(it!=_userConnMap.end()){
        it->second->send(msg);
        return;
    }

    //存储该用户的离线消息，若在取消息过程中下线了，就保存离线消息
    _offlineMsgModel.insert(userid,msg);
}
