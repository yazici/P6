#include "stdafx.h"
#include <limits>
#include <fstream>
#include <vector>
#include <Eigen/Core>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/fpfh.h>
#include <pcl/registration/ia_ransac.h>

class FeatureCloud
{
public:
	// A bit of shorthand
	typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;
	typedef pcl::PointCloud<pcl::Normal> SurfaceNormals;
	typedef pcl::PointCloud<pcl::FPFHSignature33> LocalFeatures;
	typedef pcl::search::KdTree<pcl::PointXYZ> SearchMethod;

	FeatureCloud() :
		search_method_xyz_(new SearchMethod),
		normal_radius_(0.02f),
		feature_radius_(0.02f)
	{}

	~FeatureCloud() {}

	// Process the given cloud
	void
		setInputCloud(PointCloud::Ptr xyz)
	{
		xyz_ = xyz;
		processInput();
	}

	// Load and process the cloud in the given PCD file
	void
		loadInputCloud(const std::string &pcd_file)
	{
		xyz_ = PointCloud::Ptr(new PointCloud);
		pcl::io::loadPCDFile(pcd_file, *xyz_);
		processInput();
	}

	// Get a pointer to the cloud 3D points
	PointCloud::Ptr
		getPointCloud() const
	{
		return (xyz_);
	}

	// Get a pointer to the cloud of 3D surface normals
	SurfaceNormals::Ptr
		getSurfaceNormals() const
	{
		return (normals_);
	}

	// Get a pointer to the cloud of feature descriptors
	LocalFeatures::Ptr
		getLocalFeatures() const
	{
		return (features_);
	}

protected:
	// Compute the surface normals and local features
	void
		processInput()
	{
		computeSurfaceNormals();
		computeLocalFeatures();
	}

	// Compute the surface normals
	void
		computeSurfaceNormals()
	{
		normals_ = SurfaceNormals::Ptr(new SurfaceNormals);

		pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> norm_est;
		norm_est.setInputCloud(xyz_);
		norm_est.setSearchMethod(search_method_xyz_);
		norm_est.setRadiusSearch(normal_radius_);
		norm_est.compute(*normals_);
	}

	// Compute the local feature descriptors
	void
		computeLocalFeatures()
	{
		features_ = LocalFeatures::Ptr(new LocalFeatures);

		pcl::FPFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fpfh_est;
		fpfh_est.setInputCloud(xyz_);
		fpfh_est.setInputNormals(normals_);
		fpfh_est.setSearchMethod(search_method_xyz_);
		fpfh_est.setRadiusSearch(feature_radius_);
		fpfh_est.compute(*features_);
	}

private:
	// Point cloud data
	PointCloud::Ptr xyz_;
	SurfaceNormals::Ptr normals_;
	LocalFeatures::Ptr features_;
	SearchMethod::Ptr search_method_xyz_;

	// Parameters
	float normal_radius_;
	float feature_radius_;
};

class TemplateAlignment
{
public:

	// A struct for storing alignment results
	struct Result
	{
		float fitness_score;
		Eigen::Matrix4f final_transformation;
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	};

	TemplateAlignment() :
		min_sample_distance_(0.05f),
		max_correspondence_distance_(0.01f*0.01f),
		nr_iterations_(500)
	{
		// Initialize the parameters in the Sample Consensus Initial Alignment (SAC-IA) algorithm
		sac_ia_.setMinSampleDistance(min_sample_distance_);
		sac_ia_.setMaxCorrespondenceDistance(max_correspondence_distance_);
		sac_ia_.setMaximumIterations(nr_iterations_);
	}

	~TemplateAlignment() {}

	// Set the given cloud as the target to which the templates will be aligned
	void
		setTargetCloud(FeatureCloud &target_cloud)
	{
		target_ = target_cloud;
		sac_ia_.setInputTarget(target_cloud.getPointCloud());
		sac_ia_.setTargetFeatures(target_cloud.getLocalFeatures());
	}

	// Add the given cloud to the list of template clouds
	void
		addTemplateCloud(FeatureCloud &template_cloud)
	{
		templates_.push_back(template_cloud);
	}

	// Align the given template cloud to the target specified by setTargetCloud ()
	void
		align(FeatureCloud &template_cloud, TemplateAlignment::Result &result)
	{
		sac_ia_.setInputCloud(template_cloud.getPointCloud());
		sac_ia_.setSourceFeatures(template_cloud.getLocalFeatures());

		pcl::PointCloud<pcl::PointXYZ> registration_output;
		sac_ia_.align(registration_output);

		result.fitness_score = (float)sac_ia_.getFitnessScore(max_correspondence_distance_);
		result.final_transformation = sac_ia_.getFinalTransformation();
	}

	// Align all of template clouds set by addTemplateCloud to the target specified by setTargetCloud ()
	void
		alignAll(std::vector<TemplateAlignment::Result, Eigen::aligned_allocator<Result> > &results)
	{
		results.resize(templates_.size());
		for (size_t i = 0; i < templates_.size(); ++i)
		{
			align(templates_[i], results[i]);
		}
	}

	// Align all of template clouds to the target cloud to find the one with best alignment score
	int
		findBestAlignment(TemplateAlignment::Result &result)
	{
		// Align all of the templates to the target cloud
		std::vector<Result, Eigen::aligned_allocator<Result> > results;
		alignAll(results);

		// Find the template with the best (lowest) fitness score
		float lowest_score = std::numeric_limits<float>::infinity();
		int best_template = 0;
		for (size_t i = 0; i < results.size(); ++i)
		{
			const Result &r = results[i];
			if (r.fitness_score < lowest_score)
			{
				lowest_score = r.fitness_score;
				best_template = (int)i;
			}
		}

		// Output the best alignment
		result = results[best_template];
		return (best_template);
	}

private:
	// A list of template clouds and the target to which they will be aligned
	std::vector<FeatureCloud> templates_;
	FeatureCloud target_;

	// The Sample Consensus Initial Alignment (SAC-IA) registration routine and its parameters
	pcl::SampleConsensusInitialAlignment<pcl::PointXYZ, pcl::PointXYZ, pcl::FPFHSignature33> sac_ia_;
	float min_sample_distance_;
	float max_correspondence_distance_;
	int nr_iterations_;
};

Eigen::Matrix4f toolPlacement(Eigen::Matrix3f legRotation, Eigen::Vector3f legTranslation)
{
	//Initial placement of the tool, which will then be modified according to the transformation matrix
	Eigen::Matrix4f toolPlacement;
	toolPlacement <<	0.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 1.0f;

	Eigen::Matrix4f transformationMatrix;
	transformationMatrix << legRotation(0,0), legRotation(0,1), legRotation(0,2), legTranslation(0),
							legRotation(1, 0), legRotation(1, 1), legRotation(1, 3), legTranslation(1),
							legRotation(2, 0), legRotation(2, 1), legRotation(2, 3), legTranslation(2),
							0.0f, 0.0f, 0.0f, 1.0f;

	return toolPlacement * transformationMatrix;
}

// Align a collection of object templates to a sample point cloud
int main(int argc, char **argv)
{
	// Load the object templates specified in the object_templates.txt file
	std::vector<FeatureCloud> object_templates;
	while (1)
	{
		FeatureCloud template_cloud;
		template_cloud.loadInputCloud("leg_filtered.pcd");
		object_templates.push_back(template_cloud);
		break;
	}

	// Load the target cloud PCD file
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::io::loadPCDFile("leg.pcd", *cloud);

	// Preprocess the cloud by...
	// ...removing distant points
	const float depth_limit = 1.0;
	pcl::PassThrough<pcl::PointXYZ> pass;
	pass.setInputCloud(cloud);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0, depth_limit);
	pass.filter(*cloud);

	// ... and downsampling the point cloud
	const float voxel_grid_size = 0.005f;
	pcl::VoxelGrid<pcl::PointXYZ> vox_grid;
	vox_grid.setInputCloud(cloud);
	vox_grid.setLeafSize(voxel_grid_size, voxel_grid_size, voxel_grid_size);
	//vox_grid.filter (*cloud); // Please see this http://www.pcl-developers.org/Possible-problem-in-new-VoxelGrid-implementation-from-PCL-1-5-0-td5490361.html
	pcl::PointCloud<pcl::PointXYZ>::Ptr tempCloud(new pcl::PointCloud<pcl::PointXYZ>);
	vox_grid.filter(*tempCloud);
	cloud = tempCloud;

	// Assign to the target FeatureCloud
	FeatureCloud target_cloud;
	target_cloud.setInputCloud(cloud);

	// Set the TemplateAlignment inputs
	TemplateAlignment template_align;
	for (size_t i = 0; i < object_templates.size(); ++i)
	{
		template_align.addTemplateCloud(object_templates[i]);
	}
	template_align.setTargetCloud(target_cloud);

	// Find the best template alignment
	TemplateAlignment::Result best_alignment;
	int best_index = template_align.findBestAlignment(best_alignment);
	const FeatureCloud &best_template = object_templates[best_index];

	// Print the alignment fitness score (values less than 0.00002 are good)
	printf("Best fitness score: %f\n", best_alignment.fitness_score);

	// Print the rotation matrix and translation vector
	Eigen::Matrix3f rotation = best_alignment.final_transformation.block<3, 3>(0, 0);
	Eigen::Vector3f translation = best_alignment.final_transformation.block<3, 1>(0, 3);

	printf("\n");
	printf("    | %6.3f %6.3f %6.3f | \n", rotation(0, 0), rotation(0, 1), rotation(0, 2));
	printf("R = | %6.3f %6.3f %6.3f | \n", rotation(1, 0), rotation(1, 1), rotation(1, 2));
	printf("    | %6.3f %6.3f %6.3f | \n", rotation(2, 0), rotation(2, 1), rotation(2, 2));
	printf("\n");
	printf("t = < %0.3f, %0.3f, %0.3f >\n", translation(0), translation(1), translation(2));

	// Save the aligned template for visualization
	pcl::PointCloud<pcl::PointXYZ> transformed_cloud;
	pcl::transformPointCloud(*best_template.getPointCloud(), transformed_cloud, best_alignment.final_transformation);
	pcl::io::savePCDFileBinary("output.pcd", transformed_cloud);

	return (0);
}
/*#include <iostream>
#include <vector>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/principal_curvatures.h>

#include <pcl/ModelCoefficients.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/project_inliers.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/surface/concave_hull.h>


#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/features/moment_of_inertia_estimation.h>
#include <vector>
#include <pcl/visualization/cloud_viewer.h>
#include <boost/thread/thread.hpp>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/sample_consensus/sac_model_sphere.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/console/parse.h>
#include <pcl/features/normal_3d.h>
#include <pcl/sample_consensus/sac_model_cylinder.h>
#include <pcl/filters/normal_refinement.h>
#include <pcl/common/centroid.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/conversions.h>

using namespace std;

//The function calculates curvature of a known line, which is located on the last 35% of the PointCloud, processed with concave hull
void curvatureMeasurement(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, pcl::PointCloud<pcl::Normal>::Ptr normals)
{
	float movement;


}


int main(int argc, char** argv)
{
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>),
		cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>),
		cloud_projected(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PCDReader reader;

	reader.read("leg.pcd", *cloud);
	// Build a filter to remove spurious NaNs
	pcl::PassThrough<pcl::PointXYZ> pass;
	pass.setInputCloud(cloud);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0, 1.1);
	pass.filter(*cloud_filtered);
	std::cerr << "PointCloud after filtering has: "
		<< cloud_filtered->points.size() << " data points." << std::endl;

	pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
	// Create the segmentation object
	pcl::SACSegmentation<pcl::PointXYZ> seg;
	// Optional
	seg.setOptimizeCoefficients(true);
	// Mandatory
	seg.setModelType(pcl::SACMODEL_PLANE);
	seg.setMethodType(pcl::SAC_RANSAC);
	seg.setDistanceThreshold(0.01);

	seg.setInputCloud(cloud_filtered);
	seg.segment(*inliers, *coefficients);
	std::cerr << "PointCloud after segmentation has: "
		<< inliers->indices.size() << " inliers." << std::endl;

	// Project the model inliers
	pcl::ProjectInliers<pcl::PointXYZ> proj;
	proj.setModelType(pcl::SACMODEL_PLANE);
	proj.setIndices(inliers);
	proj.setInputCloud(cloud_filtered);
	proj.setModelCoefficients(coefficients);
	proj.filter(*cloud_projected);
	std::cerr << "PointCloud after projection has: "
		<< cloud_projected->points.size() << " data points." << std::endl;

	// Create a Concave Hull representation of the projected inliers
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_hull(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::ConcaveHull<pcl::PointXYZ> chull;
	chull.setInputCloud(cloud_projected);
	chull.setAlpha(0.1);
	chull.reconstruct(*cloud_hull);

	std::cerr << "Concave hull has: " << cloud_hull->points.size()
		<< " data points." << std::endl;

	pcl::visualization::PCLVisualizer::Ptr viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));
	viewer->addPointCloud<pcl::PointXYZ>(cloud_hull, "sample cloud");

	while (!viewer->wasStopped())
	{
		viewer->spinOnce(100);
		boost::this_thread::sleep(boost::posix_time::microseconds(100000));

		/*


	int main(int, char** argv)
	{
		//std::string filename = argv[1];
		//std::cout << "Reading " << filename << std::endl;

		pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);

		if (pcl::io::loadPCDFile<pcl::PointXYZ>("leg_filtered.pcd", *cloud) == -1) //* load the file
		{
			PCL_ERROR("Couldn't read file");
			return (-1);
		}

		std::cout << "Loaded " << cloud->points.size() << " points." << std::endl;

		// Compute the normals
		pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation;
		normal_estimation.setInputCloud(cloud);

		pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
		normal_estimation.setSearchMethod(tree);

		pcl::PointCloud<pcl::Normal>::Ptr cloud_with_normals(new pcl::PointCloud<pcl::Normal>);

		normal_estimation.setRadiusSearch(0.03);

		normal_estimation.compute(*cloud_with_normals);

		// Setup the principal curvatures computation
		pcl::PrincipalCurvaturesEstimation<pcl::PointXYZ, pcl::Normal, pcl::PrincipalCurvatures> principal_curvatures_estimation;

		// Provide the original point cloud (without normals)
		principal_curvatures_estimation.setInputCloud(cloud);

		// Provide the point cloud with normals
		principal_curvatures_estimation.setInputNormals(cloud_with_normals);

		// Use the same KdTree from the normal estimation
		principal_curvatures_estimation.setSearchMethod(tree);
		principal_curvatures_estimation.setRadiusSearch(1.0);

		// Actually compute the principal curvatures
		pcl::PointCloud<pcl::PrincipalCurvatures>::Ptr principal_curvatures(new pcl::PointCloud<pcl::PrincipalCurvatures>());
		principal_curvatures_estimation.compute(*principal_curvatures);

		std::cout << "output points.size (): " << principal_curvatures->points.size() << std::endl;

		// Display and retrieve the shape context descriptor vector for the 0th point.
		pcl::PrincipalCurvatures descriptor = principal_curvatures->points[0];
		std::cout << descriptor << std::endl;
	}
	return 0;
}
*/