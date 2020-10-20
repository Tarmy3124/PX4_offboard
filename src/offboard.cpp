/**
 * @file offb_node.cpp
 * @brief offboard example node, written with mavros version 0.14.2, px4 flight
 * stack and tested in Gazebo SITL
 */

#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/SetMode.h>
#include <mavros_msgs/State.h>
#include <rosmsg_template/simple.h>
#include <string>
using namespace std;
const unsigned char *pregetkey;
char getkey[20]={};
string keystring;
mavros_msgs::State current_state;
void keyvalue_cb(const rosmsg_template::simple::ConstPtr &msg)
{
// cout<<"key value is "<<msg->keystring<<endl;
//strcpy(getkey,&msg.keystring[0]);
pregetkey=&msg->keystring[0];
//for循环转换格式
for(int i=0;i<sizeof(pregetkey);i++)
{
 getkey[i]=pregetkey[i];
}
//keystring =(string *)&msg->keystring[0];
cout<<"what i get "<<getkey<<endl;
}
void state_cb(const mavros_msgs::State::ConstPtr& msg){
    current_state = *msg;
    cout<<"Px4 current state is "<<current_state.mode<<endl;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "offb_node"); //节点重命名为offb_node 供ROS调用
    ros::NodeHandle nh; // 打开roscpp节点
  //  ros::Subscriber key_sub = nh.subscribe<rosmsg_template::simple>("rosmsg_template", 100, keyvalue_cb);
  ros::Subscriber key_sub = nh.subscribe<rosmsg_template::simple>("rosmsg_template", 10, keyvalue_cb);
    ros::Subscriber state_sub = nh.subscribe<mavros_msgs::State>
            ("mavros/state", 10, state_cb);// 订阅函数的参数1表示主题  参数2表示缓存的消息队列长度  消息3表示消息更新时调用的回调函数
    ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped> // 公告发布者的主题，参数2表示输出到订阅者的最大消息队列长度
            ("mavros/setpoint_position/local", 10);
    ros::ServiceClient arming_client = nh.serviceClient<mavros_msgs::CommandBool>
            ("mavros/cmd/arming"); // 创建一个客户端用来解锁
    ros::ServiceClient set_mode_client = nh.serviceClient<mavros_msgs::SetMode>
            ("mavros/set_mode"); // 创建一个客户端来设置模式
    ros::ServiceClient set_submode_client = nh.serviceClient<mavros_msgs::SetMode>
            ("mavros/set_mode"); // 创建一个客户端来降落
        ros::ServiceClient set_submode_loiter = nh.serviceClient<mavros_msgs::SetMode>
            ("mavros/set_mode"); // 创建一个客户端来loiter

    //the setpoint publishing rate MUST be faster than 2Hz
    ros::Rate rate(20.0);

    // wait for FCU connection
   while(ros::ok() && current_state.connected){
        cout<<"connected"<<endl;
        ros::spinOnce();
        rate.sleep();
    }

    geometry_msgs::PoseStamped pose;
    pose.pose.position.x = 0;
    pose.pose.position.y = 0;
    pose.pose.position.z = 2;

    //send a few setpoints before starting
    for(int i = 100; ros::ok() && i > 0; --i){
        local_pos_pub.publish(pose);
        ros::spinOnce();
        rate.sleep(); // 保证更新频率为20Hz，自动调整睡眠时间
    }

    mavros_msgs::SetMode offb_set_mode,land_set_mode,loiter_set_mode;
    land_set_mode.request.custom_mode = "AUTO.LAND";
    loiter_set_mode.request.custom_mode = "AUTO.LOITER";
    offb_set_mode.request.custom_mode = "OFFBOARD"; // 调用者请求

    mavros_msgs::CommandBool arm_cmd;
    arm_cmd.request.value = true;

    ros::Time last_request = ros::Time::now();

    while(ros::ok()){
      
          if(strncmp(getkey,"land",strlen("land"))==0){
            strcpy(getkey , "nothing to do"); //for null
             if( set_submode_client.call(land_set_mode) && land_set_mode.response.mode_sent){ // 被调用者响应
                ROS_INFO("landing ....");
            }
         }
         if(strncmp(getkey,"loiter", strlen("loiter"))==0){ //loiter
             strcpy(getkey , "nothing to do"); //for null
             if( set_submode_loiter.call(loiter_set_mode) && loiter_set_mode.response.mode_sent){ // 被调用者响应
                ROS_INFO("loiter ....");
            }
         }
     if(strncmp(getkey,"offboard", strlen("offboard"))==0){
        strcpy(getkey,"nothing to do");
        ROS_INFO("Offboard ready to takeoff");
        if( current_state.mode != "OFFBOARD" &&
            (ros::Time::now() - last_request > ros::Duration(5.0))){
            if( set_mode_client.call(offb_set_mode) && offb_set_mode.response.mode_sent){ // 被调用者响应
                ROS_INFO("Offboard enabled");
            }
            last_request = ros::Time::now();
        } else {
            if( !current_state.armed &&
                (ros::Time::now() - last_request > ros::Duration(5.0))){
                if( arming_client.call(arm_cmd) &&
                    arm_cmd.response.success){
                    ROS_INFO("Vehicle armed");
                }
                last_request = ros::Time::now();
        }
      }
    }
        local_pos_pub.publish(pose);
//ros::spin();
        ros::spinOnce();
        rate.sleep();
    }

    return 0;
}
