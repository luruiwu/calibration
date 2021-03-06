/**************************************************************************************************
   Software License Agreement (BSD License)

   Copyright (c) 2011-2013, LAR toolkit developers - University of Aveiro - http://lars.mec.ua.pt
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted
   provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of
   conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other materials provided
   with the distribution.
 * Neither the name of the University of Aveiro nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
   IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************************************/
/**
   \file  lms1_kinect_pg_fusion.cpp
   \brief Performs data fusion with the following sensors:
   - SICK LMS151;
   - Microsoft Kinect 3D-depth sensor;
   - Point Grey FL3-GE28S4-C.
   \author David Silva
   \date   June, 2016
 */

#include "multisensor_fusion/subscribers_class.h"
#include "multisensor_fusion/multisensor_fusion_class.h"


/**
   @brief Main function which performs data fusion with the following sensors:
   - SICK LMS151;
   - Microsoft Kinect 3D-depth sensor;
   - Point Grey FL3-GE28S4-C.
   @param argc
   @param argv
   @return int
 */
int main(int argc, char** argv)
{
	ros::init(argc, argv, "lms1_kinect_pg");
	ros::NodeHandle node;

	image_transport::ImageTransport it(node);
	image_transport::Publisher fusion_image_pub = it.advertise("image_fusion", 3);
	sensor_msgs::ImagePtr image_msg;
	image_transport::Publisher fusion_image_pnp_pub = it.advertise("image_fusion_pnp", 3);
	sensor_msgs::ImagePtr image_msg_pnp;

	// get calibration package path and navigate to folder where the geometric transformations are kept
	std::string pkg_path = ros::package::getPath("multisensor_fusion") + "/geometric_transf/lms1_kinect_pg/";

	std::vector<std::string> files;

	// Get transformation for each sensor
	std::string file_path;
	// 3D rigid transforms
	file_path = pkg_path + "lms151_1_pointgrey_1_calib.txt";
	files.push_back(file_path);

	// Extrinsic camera calibration
	file_path = pkg_path + "lms151_1_pointgrey_1_solvePnPRansac_calib.txt";
	files.push_back(file_path);

  file_path = pkg_path + "lms151_1_kinect_3d_1_calib.txt";
  files.push_back(file_path);

	//read calibration paraneters
	std::string path = ros::package::getPath("multisensor_fusion") + "/intrinsic_calibrations/ros_calib.yaml";
	cv::FileStorage fs(path, cv::FileStorage::READ);
	if(!fs.isOpened())
	{
		std::cout << "failed to intrisic parameters file" << std::endl;
		return -1;
	}

	// Matrices to store intrinsic parameters and distortion coefficients
	cv::Mat intrinsic_matrix, distortion_coeffs;

	fs["CM"] >> intrinsic_matrix;
	fs["D"] >> distortion_coeffs;


	MultisensorFusion fusion_helper(files);

	MultisensorSubs multisensor_subs(intrinsic_matrix, distortion_coeffs);


	// LMS151 - reference sensor ================================================
	tf::Quaternion q;
	tf::TransformBroadcaster br_lms151a;
	tf::Transform transform_lms151a;

	transform_lms151a.setOrigin(tf::Vector3(0.0, 0.0, 0.0));
	q.setRPY(0, 0, 0);

	transform_lms151a.setRotation(q);

	// Non-reference sensors ====================================================
	std::vector<tf::Transform> tf_transforms = fusion_helper.getTFTranforms();
	std::vector<tf::TransformBroadcaster> tf_broadcasters = fusion_helper.getTFBroadcasters();


	std::vector<cv::Mat> transformations = fusion_helper.getCVTransformations();

	cv::Mat pointgrey_lms151_1 = transformations[0].inv();

	cv::Mat pointgrey_lms151_1_pnp = transformations[1].inv();

	cv::Mat fusion_img;
	cv::Mat fusion_img_pnp;

	ros::Rate rate(10.0);
	while (node.ok())
	{
		br_lms151a.sendTransform(tf::StampedTransform(transform_lms151a, ros::Time::now(),"lms1_kinect_pg","lms151_1"));

		// For depth registered point cloud (XYZ+RGB)
		tf_broadcasters[2].sendTransform(tf::StampedTransform(tf_transforms[2], ros::Time::now(),"lms1_kinect_pg","camera_rgb_optical_frame"));
		// For point cloud (XYZ)
		//tf_broadcasters[2].sendTransform(tf::StampedTransform(tf_transforms[2], ros::Time::now(),"lms1_kinect_pg","camera_depth_optical_frame"));

    fusion_img = multisensor_subs.camImage_rect.clone();
		fusion_img_pnp = multisensor_subs.camImage_rect.clone();

		if (multisensor_subs.lms1CVpoints.size())
		{
			fusion_img = fusion_helper.project3DtoImage(fusion_img, pointgrey_lms151_1, multisensor_subs.lms1CVpoints,
			                                            intrinsic_matrix, distortion_coeffs, cv::Scalar( 0, 0, 255));

			fusion_img_pnp = fusion_helper.project3DtoImage(fusion_img_pnp, pointgrey_lms151_1_pnp, multisensor_subs.lms1CVpoints,
			                                                intrinsic_matrix, distortion_coeffs, cv::Scalar( 0, 0, 255));
		}
		if(!fusion_img.empty()) {
			image_msg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", fusion_img).toImageMsg();
			fusion_image_pub.publish(image_msg);
			image_msg_pnp = cv_bridge::CvImage(std_msgs::Header(), "bgr8", fusion_img_pnp).toImageMsg();
			fusion_image_pnp_pub.publish(image_msg_pnp);
		}
		ros::spinOnce();
		rate.sleep();
	}

	return 0;
}
