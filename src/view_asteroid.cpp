// Cpp libraries
#include <ros/ros.h>
#include <visualization_msgs/MarkerArray.h>
#include <Eigen/Dense>
#include <tf/transform_broadcaster.h>
#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>
#include "view_asteroid/helper.h"
#include <thread>

// Global variables
ros::Publisher pub_vis;	 // Publisher for visualization in RVIZ

ros::Time tf_pub(const Eigen::Vector3d &point,
	             const Eigen::Quaterniond &quat,
	             const std::string parent_frame,
	             const std::string child_frame){
  static tf::TransformBroadcaster br;
  // ROS_INFO("Publishing into frame: %s", msg->header.frame_id.c_str());
  tf::Transform transform;
  transform.setOrigin( tf::Vector3(point[0], point[1], point[2]));
  tf::Quaternion q(quat.x(), quat.y(), quat.z(), quat.w());
  transform.setRotation(q);
  ros::Time time_now = ros::Time::now();
  br.sendTransform(
  		tf::StampedTransform(transform, time_now,
                             parent_frame, child_frame));
  return time_now;
}

ros::Time tf_pub(const geometry_msgs::Point &point,
	             const geometry_msgs::Quaternion &quat,
	             const std::string parent_frame,
	             const std::string child_frame){
  static tf::TransformBroadcaster br;
  // ROS_INFO("Publishing into frame: %s", msg->header.frame_id.c_str());
  tf::Transform transform;
  transform.setOrigin( tf::Vector3(point.x, point.y, point.z));
  tf::Quaternion q(quat.x, quat.y, quat.z, quat.w);
  transform.setRotation(q);
  ros::Time time_now = ros::Time::now();
  br.sendTransform(
  		tf::StampedTransform(transform, time_now,
                             parent_frame, child_frame));
  return time_now;
}


void MeshMarker(const Eigen::Vector3d &point,
	            const Eigen::Quaterniond &quat,
	            const std::string &frame_id,
	            const std::string &ns,  // namespace
	            const std::string &file_3d,
	            const double &size,
	            const int &seqNumber,
	            visualization_msgs::MarkerArray *markerArray) {
	visualization_msgs::Marker marker;
	marker.type = visualization_msgs::Marker::MESH_RESOURCE;
	marker.mesh_resource = "package://view_asteroid/meshes/" + file_3d;
	marker.action = visualization_msgs::Marker::ADD;
	// marker.color = color;
	marker.color.r = 0.0;
	marker.color.g = 0.0;
	marker.color.b = 0.0;
	marker.color.a = 0.0;
	marker.scale.x = size;
	marker.scale.y = size;
	marker.scale.z = size;
	marker.ns = ns;
	marker.header.frame_id = frame_id;
	marker.header.stamp = ros::Time::now();
	marker.pose.orientation.w = quat.w();
	marker.pose.orientation.x = quat.x();
	marker.pose.orientation.y = quat.y();
	marker.pose.orientation.z = quat.z();
	marker.mesh_use_embedded_materials = true;

	geometry_msgs::Point position;
	position.x = size*point(0);
	position.y = size*point(1);
	position.z = size*point(2);
	marker.pose.position = position;
	marker.id = seqNumber;
	// marker.lifetime = ros::Duration(1.0);
	markerArray->markers.push_back(marker);
}

void poseCallback(const geometry_msgs::Pose::ConstPtr& msg,
	              visualization_msgs::MarkerArray& asteroid_marker) {
	// Send object pose to tf tree
	ros::Time time_now = tf_pub(msg->position, 
		                        msg->orientation, 
		                        "world", "asteroid");
	// Asteroid visualization
	asteroid_marker.markers[0].header.stamp = time_now;
	pub_vis.publish(asteroid_marker);
}

void camCallback(const geometry_msgs::Pose::ConstPtr& msg,
	             const double& cam_baseline) {
	// Set camera's tree frame
	// Eigen::Vector3d cam_position(cam_pos[0], cam_pos[1], cam_pos[2]);
	tf_pub(msg->position, msg->orientation, "world", "camera");

	// Set camera rotations w.r.t. camera frame
	Eigen::Quaterniond q_cam1(cos(M_PI/4.0), 0.0, sin(M_PI/4.0), 0.0);
	Eigen::Quaterniond q_cam2(cos(M_PI/4.0), 0.0, 0.0, -sin(M_PI/4.0));
	Eigen::Quaterniond q_cam = q_cam1*q_cam2;

	// Set frame for camera 1 w.r.t. the camera frame
	Eigen::Vector3d cam1_position(0.0, 0.0, 0.0);
	tf_pub(cam1_position, q_cam, "camera", "camera1");

	// Set frame for camera 2 w.r.t. the camera frame
	Eigen::Vector3d cam2_position(0.0, cam_baseline, 0.0);
	tf_pub(cam2_position, q_cam, "camera", "camera2");
}

int main(int argc, char** argv){
	ros::init(argc, argv, "view_asteroid");
	ros::NodeHandle node("~");
	ROS_INFO("Asteroid view started!");
	const double rate = 200.0;
	ros::Rate loop_rate(rate);

	// Get camera baseline
	double cam_baseline;
	node.getParam("cam_baseline", cam_baseline);

	// Get 3d file for rendering
	std::string file_3d;
	node.getParam("file_3d", file_3d);

	// Get scaling and offset for rendered object
	std::vector<double> offset;
	double scale;
	node.getParam("offset_object", offset);
	node.getParam("scale_object", scale);
	
	// Get object mesh, offset and scale it
	Eigen::Vector3d obj_offset(offset[0], offset[1], offset[2]);
	Eigen::Vector3d origin(0.0, 0.0, 0.0);
	Eigen::Quaterniond q_0(1.0, 0.0, 0.0, 0.0);
	std::string frame_id = "asteroid";
	std::string ns = "asteroid";
	double size = scale;
	int seq_number = 1;
	visualization_msgs::MarkerArray asteroid_marker;
	MeshMarker(obj_offset, q_0, frame_id, ns, file_3d,
	           size, seq_number, &asteroid_marker);
	pub_vis = node.advertise
		<visualization_msgs::MarkerArray>("asteroid_marker", 1);

	// Set subscribers to object and camera pose
	ros::Subscriber obj_subs, cam_subs;
	std::string obj_pose_topic, cam_pose_topic;
	node.getParam("object_pose_topic", obj_pose_topic);
	node.getParam("camera_pose_topic", cam_pose_topic);
	ros::Subscriber obj_sub = node.subscribe<geometry_msgs::Pose>(obj_pose_topic, 10, 
		boost::bind(poseCallback, _1, asteroid_marker));
	ros::Subscriber cam_sub = node.subscribe<geometry_msgs::Pose>(cam_pose_topic, 10, 
		boost::bind(camCallback, _1, cam_baseline));

	ROS_INFO("[view_asteroid]: Subscribing to: %s", obj_pose_topic.c_str());
    ROS_INFO("[view_asteroid]: Subscribing to: %s", cam_pose_topic.c_str());
    
	
	// double omega = 0.0;
	// ros::Time t0 = ros::Time::now();
	// while (ros::ok()) {

		// // Set asteroid's orientation
		// float dt = (ros::Time::now() - t0).toSec();
		// float roll = omega*dt, pitch = -M_PI/2.0, yaw = 0.0;
		// Eigen::Quaterniond q1, q2, q;
		// Eigen::Matrix3f m;
		// q1 = Eigen::AngleAxisd(pitch, Eigen::Vector3d::UnitY());
		// q2 = Eigen::AngleAxisd(roll, Eigen::Vector3d::UnitX());
		// q = q1*q2;

		// // Send asteroid pose
		// ros::Time time_now = tf_pub(origin, q, "world", "asteroid");

		// // Set camera's tree frame
		// Eigen::Vector3d cam_position(-2.0, 0.0, 0.0);
		// tf_pub(cam_position, q_0, "world", "camera");

		// // Set frame for camera 1 w.r.t. the camera frame
		// Eigen::Quaterniond q_cam1(cos(M_PI/4.0), 0.0, sin(M_PI/4.0), 0.0);
		// Eigen::Quaterniond q_cam2(cos(M_PI/4.0), 0.0, 0.0, -sin(M_PI/4.0));
		// Eigen::Quaterniond q_cam = q_cam1*q_cam2;
		// Eigen::Vector3d cam1_position = origin;
		// tf_pub(cam1_position, q_cam, "camera", "camera1");

		// // Set frame for camera 2 w.r.t. the camera frame
		// Eigen::Vector3d cam2_position(0.0, cam_baseline, 0.0);
		// tf_pub(cam2_position, q_cam, "camera", "camera2");
		
		// // Asteroid visualization
		// asteroid_marker.markers[0].header.stamp = time_now;
		// pub_vis.publish(asteroid_marker);

	// 	loop_rate.sleep();
	// }
	

	// ROS loop that starts callbacks/publishers
	while (ros::ok()) {
		ros::spinOnce();
		loop_rate.sleep();
	}

	return 0;
}