#include <rclcpp/rclcpp.hpp>
#include "harvest_interfaces/srv/move_to_pose.hpp"
#include "harvest_interfaces/srv/send_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"
#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <moveit/robot_trajectory/robot_trajectory.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <builtin_interfaces/msg/duration.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include "std_srvs/srv/empty.hpp"

// Read data packages
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cmath> // For M_PI

using std::placeholders::_1;
using std::placeholders::_2;
using namespace std::chrono_literals;

class MoveArmNode : public rclcpp::Node
{
public:
    MoveArmNode();
    std::vector<std::vector<std::vector<double>>> read_data(const std::string &filename, int dim1, int dim2, int dim3);
    void set_to_home();
    void printToolExtensionLocation();

private:
    rclcpp::Service<harvest_interfaces::srv::SendTrajectory>::SharedPtr arm_trajectory_service_;
    rclcpp::Service<harvest_interfaces::srv::MoveToPose>::SharedPtr arm_to_pose_service_;
    rclcpp::Service<std_srvs::srv::Empty>::SharedPtr arm_to_home_service_;

    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    moveit::planning_interface::MoveGroupInterface move_group_;

    std::vector<double> home_joint_positions = {
        M_PI / 2,
        -2 * M_PI / 3,
        2 * M_PI / 3,
        -M_PI,
        -M_PI / 2,
        0};

    void execute_trajectory(const std::shared_ptr<harvest_interfaces::srv::SendTrajectory::Request> request,
                            std::shared_ptr<harvest_interfaces::srv::SendTrajectory::Response> response);
    void move_to_pose(const std::shared_ptr<harvest_interfaces::srv::MoveToPose::Request> request,
                      std::shared_ptr<harvest_interfaces::srv::MoveToPose::Response> response);
    void move_to_home(const std::shared_ptr<std_srvs::srv::Empty::Request> request,
                      std::shared_ptr<std_srvs::srv::Empty::Response> response);
};

MoveArmNode::MoveArmNode()
    : Node("move_arm_node", rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true)),
      tf_buffer_(std::make_shared<tf2_ros::Buffer>(this->get_clock())),
      tf_listener_(std::make_shared<tf2_ros::TransformListener>(*tf_buffer_)),
      move_group_(std::shared_ptr<rclcpp::Node>(std::move(this)), "ur_manipulator")
{
    // Set up services
    arm_trajectory_service_ = this->create_service<harvest_interfaces::srv::SendTrajectory>(
        "execute_arm_trajectory", std::bind(&MoveArmNode::execute_trajectory, this, _1, _2));

    arm_to_pose_service_ = this->create_service<harvest_interfaces::srv::MoveToPose>(
        "move_arm_to_pose", std::bind(&MoveArmNode::move_to_pose, this, _1, _2));

    arm_to_home_service_ = this->create_service<std_srvs::srv::Empty>(
        "move_arm_to_home", std::bind(&MoveArmNode::move_to_home, this, _1, _2));

    // Set velocity and acceleration limits
    // NEED TO RESET TO 0.1 FOR HARDWARE!!!
    this->move_group_.setMaxAccelerationScalingFactor(0.1);
    this->move_group_.setMaxVelocityScalingFactor(0.1);

    // // Set the arm to the home position
    // set_to_home();

    RCLCPP_INFO(this->get_logger(), "Move arm server ready");
}

void MoveArmNode::set_to_home()
{
    // Set the home configuration as the target for the MoveGroup
    move_group_.setJointValueTarget(home_joint_positions);

    // Plan and execute to move to the home position
    moveit::planning_interface::MoveGroupInterface::Plan plan;
    bool success = static_cast<bool>(move_group_.plan(plan));

    if (success)
    {
        move_group_.execute(plan);
        RCLCPP_INFO(this->get_logger(), "Moved to home configuration.");
    }
    else
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to move to home configuration.");
    }
}

void MoveArmNode::move_to_home(const std::shared_ptr<std_srvs::srv::Empty::Request> request,
                               const std::shared_ptr<std_srvs::srv::Empty::Response> response)
{
    // Set the home configuration as the target for the MoveGroup
    move_group_.setJointValueTarget(home_joint_positions);

    // Plan and execute to move to the home position
    moveit::planning_interface::MoveGroupInterface::Plan plan;
    bool success = static_cast<bool>(move_group_.plan(plan));

    if (success)
    {
        move_group_.execute(plan);
        RCLCPP_INFO(this->get_logger(), "Moved to home configuration.");
        // response->result = true;
    }
    else
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to move to home configuration.");
        // response->result = false;
    }
}

void MoveArmNode::move_to_pose(const std::shared_ptr<harvest_interfaces::srv::MoveToPose::Request> request,
                               const std::shared_ptr<harvest_interfaces::srv::MoveToPose::Response> response)
{
    // Make posestamped message
    geometry_msgs::msg::PoseStamped msg;
    msg.header.frame_id = "base_link";
    msg.pose.position.x = request->position.x;
    msg.pose.position.y = request->position.y;
    msg.pose.position.z = request->position.z;

    tf2::Quaternion orientation;
    // orientation = 

    orientation.setRPY(3.14/2, 0, 0);
    msg.pose.orientation = tf2::toMsg(orientation);;

    // msg.pose.orientation.x = request->orientation.x;
    // msg.pose.orientation.y = request->orientation.y;
    // msg.pose.orientation.z = request->orientation.z;
    // msg.pose.orientation.w = request->orientation.w;
    // this->move_group_.setEndEffectorLink("tool_extension");
    this->move_group_.setPoseTarget(msg, "tool0");
    this->move_group_.setGoalOrientationTolerance(.1);

    // Attempts to move to pose goal
    moveit::planning_interface::MoveGroupInterface::Plan goal;
    auto const ok = static_cast<bool>(move_group_.plan(goal));
    response->result = true;
    if (ok)
    {
        this->move_group_.execute(goal);

        // Now print the current location of the tool_extension frame
        printToolExtensionLocation();
    }
    else
    {
        RCLCPP_ERROR(this->get_logger(), "Planing failed!");
        response->result = false;
    }
}

void MoveArmNode::printToolExtensionLocation()
{
    try
    {
        // Get the transform from base_link to tool_extension
        geometry_msgs::msg::TransformStamped transform_stamped;
        transform_stamped = tf_buffer_->lookupTransform("base_link", "tool0", tf2::TimePointZero);

        // Print the current position and orientation of tool_extension
        RCLCPP_INFO(this->get_logger(), "Tool Extension Position: (x: %f, y: %f, z: %f)",
                    transform_stamped.transform.translation.x,
                    transform_stamped.transform.translation.y,
                    transform_stamped.transform.translation.z);

        RCLCPP_INFO(this->get_logger(), "Tool Extension Orientation: (x: %f, y: %f, z: %f, w: %f)",
                    transform_stamped.transform.rotation.x,
                    transform_stamped.transform.rotation.y,
                    transform_stamped.transform.rotation.z,
                    transform_stamped.transform.rotation.w);
    }
    catch (tf2::TransformException &ex)
    {
        RCLCPP_ERROR(this->get_logger(), "Could not transform: %s", ex.what());
    }
}

void MoveArmNode::execute_trajectory(const std::shared_ptr<harvest_interfaces::srv::SendTrajectory::Request> request,
                                     std::shared_ptr<harvest_interfaces::srv::SendTrajectory::Response> response)
{
    // // Set the robot to home configuration
    // set_to_home();

    // Add a delay after setting to home
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Extract the layout dimensions from the Float32MultiArray message
    const auto &layout = request->waypoints.layout;
    if (layout.dim.size() < 2)
    {
        RCLCPP_ERROR(this->get_logger(), "Invalid dimensions in waypoints array");
        response->success = false;
        return;
    }

    int num_waypoints = layout.dim[0].size;
    int num_joints = layout.dim[1].size;

    // Ensure the flattened data size matches the expected dimensions
    if (static_cast<int>(request->waypoints.data.size()) != num_waypoints * num_joints)
    {
        RCLCPP_ERROR(this->get_logger(), "Mismatch between data size and dimensions");
        response->success = false;
        return;
    }

    // Create a 2D vector to store the reconstructed data
    std::vector<std::vector<double>> path(num_waypoints, std::vector<double>(num_joints));

    // Copy data into the 2D vector
    size_t index = 0;
    for (int row = 0; row < num_waypoints; ++row)
    {
        for (int col = 0; col < num_joints; ++col)
        {
            path[row][col] = request->waypoints.data[index++];
        }
    }

    for (int row = 0; row < num_waypoints; ++row)
    {
        std::stringstream ss;
        ss << "Waypoint " << row << ": ";
        for (int col = 0; col < num_joints; ++col)
        {
            ss << path[row][col];
            if (col < num_joints - 1)
            {
                ss << ", ";
            }
        }
        RCLCPP_INFO(this->get_logger(), ss.str().c_str());
    }

    // Prepare the JointTrajectory message
    trajectory_msgs::msg::JointTrajectory joint_trajectory;
    joint_trajectory.header.frame_id = move_group_.getPlanningFrame();
    joint_trajectory.joint_names = move_group_.getJointNames();

    trajectory_msgs::msg::JointTrajectoryPoint point;
    point.time_from_start.sec = 0;
    point.time_from_start.nanosec = 0;

    double current_time = 0.0; // Start time
    double time_step = 0.02;   // Time step between waypoints

    for (int i = 0; i < num_waypoints; ++i)
    {
        // Set joint positions for this point
        point.positions.clear();
        for (int j = 0; j < num_joints; ++j)
        {
            point.positions.push_back(path[i][j]);
        }

        // Set time_from_start for this point
        builtin_interfaces::msg::Duration duration;
        duration.sec = static_cast<uint32_t>(current_time);
        duration.nanosec = static_cast<uint32_t>((current_time - static_cast<uint32_t>(current_time)) * 1e9);
        point.time_from_start = duration;

        joint_trajectory.points.push_back(point);

        current_time += time_step; // Increment time for the next waypoint
    }

    // Convert JointTrajectory to RobotTrajectory
    moveit_msgs::msg::RobotTrajectory robot_trajectory;
    robot_trajectory.joint_trajectory = joint_trajectory;

    moveit::planning_interface::MoveGroupInterface::Plan plan;
    plan.trajectory_ = robot_trajectory;

    // Plan and execute the trajectory
    bool success = static_cast<bool>(move_group_.execute(plan));
    if (success)
    {
        response->success = true;
    }
    else
    {
        RCLCPP_ERROR(this->get_logger(), "Failed to execute trajectory");
        response->success = false;
    }
}

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto move_service = std::make_shared<MoveArmNode>();
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(move_service);
    executor.spin();
    rclcpp::shutdown();
    return EXIT_SUCCESS;
}