#include "stdafx.h"
#include <pcl/range_image/range_image.h>
#include <math.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/ply_io.h>
#include <pcl/features/normal_3d.h>
#include <iostream>
#include <pcl/sample_consensus/sac_model_line.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>

using namespace std;

int main(int argc, char** argv) {
	// load point cloud
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);

	pcl::io::loadPCDFile("concaveboi.pcd", *cloud); //hulltest.pdc
	cout << cloud->points.size() << endl;

	pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
	ne.setViewPoint(1, 2, 4);
	ne.setInputCloud(cloud);

	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
	ne.setSearchMethod(tree);

	// Output datasets
	pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);



	// Use all neighbors in a sphere of radius 3cm
	ne.setRadiusSearch(0.03);

	// Compute the features
	ne.compute(*normals);
	//for(int k = 0; k<normals->points.size(); k++)
	//{

	pcl::ModelCoefficients::Ptr line_coefficients(new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr inliers(new pcl::PointIndices);

	// Populate point cloud...

	///////////////////////////////////////////////////////////////////////////
	// Try out different pairs, looking for the longest distance between them
	float longest = 0.0;
	float len = 0.0;
	int ind1 = 0;
	int ind2 = 0;

	for (int i = 0; i < cloud->points.size() / 2; ++i)
	{
		float xdist = cloud->points[i].x - cloud->points[cloud->points.size() / 2 + i].x;
		float ydist = cloud->points[i].y - cloud->points[cloud->points.size() / 2 + i].y;
		float zdist = cloud->points[i].z - cloud->points[cloud->points.size() / 2 + i].z;
		len = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));

		if (len>longest)
		{
			longest = len;
			ind1 = i;
			ind2 = cloud->points.size() / 2 + i;
		}
	}
	// Try with shifted ind1
	for (int i = 0; i < cloud->points.size() / 2; ++i)
	{
		if ((i % 2) && (cloud->points.size() / 2 + i + 10 <= cloud->points.size()) && (cloud->points.size() / 2 + i + 10 >= 0))
		{
			float xdist = cloud->points[i].x - cloud->points[cloud->points.size() / 2 + i + 10].x;
			float ydist = cloud->points[i].y - cloud->points[cloud->points.size() / 2 + i + 10].y;
			float zdist = cloud->points[i].z - cloud->points[cloud->points.size() / 2 + i + 10].z;
			len = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));
		}
		else {
			float xdist = cloud->points[i].x - cloud->points[cloud->points.size() / 2 + i - 10].x;
			float ydist = cloud->points[i].y - cloud->points[cloud->points.size() / 2 + i - 10].y;
			float zdist = cloud->points[i].z - cloud->points[cloud->points.size() / 2 + i - 10].z;
			len = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));
		}

		if (len>longest)
		{
			longest = len;
			ind1 = i;
			if (i % 2)
			{
				ind2 = cloud->points.size() / 2 + i + 10;
			}
			else
			{
				ind2 = cloud->points.size() / 2 + i - 10;
			}

		}
	}


	cout << "The longest line goes between points [" << ind1 << "," << ind2
		<< "] and has length: " << longest << endl;

	//////////////////////////////////////////////////////////////////////////////////////
	// Create the segmentation object 
	pcl::SACSegmentation<pcl::PointXYZ> seg;
	seg.setModelType(pcl::SACMODEL_LINE);
	seg.setMethodType(pcl::SAC_RANSAC);
	seg.setDistanceThreshold(0.03);
	seg.setInputCloud(cloud);
	seg.segment(*inliers, *line_coefficients);
	const auto pt_line_x = line_coefficients->values[0] = cloud->points[ind1].x;
	const auto pt_line_y = line_coefficients->values[1] = cloud->points[ind1].y;
	const auto pt_line_z = line_coefficients->values[2] = cloud->points[ind1].z;
	const auto pt_direction_x = line_coefficients->values[3] = cloud->points[cloud->points.size() / 2 + ind1].x;
	const auto pt_direction_y = line_coefficients->values[4] = cloud->points[cloud->points.size() / 2 + ind1].y;
	const auto pt_direction_z = line_coefficients->values[5] = cloud->points[cloud->points.size() / 2 + ind1].z;
	cout << "line x = " << pt_line_x << " line y = " << pt_line_y << " line z = " << pt_line_z << endl;
	cout << "direction x = " << pt_direction_x << " direction y = " << pt_direction_y << " direction z = " << pt_direction_z << endl;
	//cout << normals->points[k] << endl;
	//}
	//cout << normals->points[0] << endl;
	//cout << normals->points[cloud->points.size()/2] << endl;
	pcl::visualization::PCLVisualizer viewer("PCL Viewer");
	viewer.setBackgroundColor(0.0, 0.0, 0);
	viewer.addPointCloud<pcl::PointXYZ>(cloud, "sample cloud");

	/* pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
	pcl::SACSegmentation<pcl::PointXYZ> seg;
	seg.setModelType (pcl::SACMODEL_PLANE);
	coefficients->values[0] = cloud->points[0].x;
	coefficients->values[1] = cloud->points[0].y;
	coefficients->values[2] = cloud->points[0].z;
	coefficients->values[3] = cloud->points[cloud->points.size()/2].x;
	coefficients->values[4] = cloud->points[cloud->points.size()/2].y;
	coefficients->values[5] = cloud->points[cloud->points.size()/2].z;
	*/

	/*
	viewer.addLine(cloud->points[0], cloud->points[10000], 1, 0, 0);*/

	for (int i = 0; i < cloud->points.size() - 1; i++) {

		stringstream ss;
		ss << i;
		string str = ss.str();
		viewer.addLine(cloud->points[i], cloud->points[i + 1], 1, 1, 1, str);
		/* if(cloud->points[i].x == -0.238441 && cloud->points[i].x == -0.238238)
		{
		viewer.addLine(cloud->points[i], cloud->points[i], 1, 0, 0, "p");
		cout << "line" << endl;
		}*/
		//viewer.addLine(cloud->points[26], cloud->points[27], 1, 1, 1, "g");
		//viewer.addLine(cloud->points[1], cloud->points[2], 1, 0, 0, "h");
		//viewer.addLine(cloud->points[2], cloud->points[3], 0, 0, 1, "j");
	}
	viewer.addPointCloudNormals<pcl::PointXYZ, pcl::Normal>(cloud, normals);
	viewer.addLine(cloud->points[0], cloud->points[cloud->points.size() / 2], 1, 0, 0, "q");

	//viewer.addLine(normals->points[0], normals->points[normals->points.size()-1], 1,0 ,0, "p");
	while (!viewer.wasStopped())
	{
		viewer.spinOnce();
	}
	return 0;
}
