#include <pcl/range_image/range_image.h> //
#include <math.h>
#include <vector> //
#include <pcl/io/io.h> //

#include <pcl/io/pcd_io.h>
//#include <pcl/features/integral_image_normal.h>
#include <pcl/visualization/cloud_viewer.h>
//#include <pcl/filters/voxel_grid.h>
//#include <pcl/io/ply_io.h>
//#include <pcl/features/normal_3d.h>
#include <iostream>

using namespace std;
// Refine estimate combination
// Check tool orientation
// Implement in ROS
// Integrate everything

float Dist(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int index1, int index2);
float Dist(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int index1);

class Pose {
private:
    float pose[6];

public:
void Set_values(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int start1, int end1, int start2, int end2){
// make the point indicating position
pose[0] = cloud->points[end1].x + (cloud->points[start1].x - cloud->points[end1].x)/2;
pose[1] = cloud->points[end1].y + (cloud->points[start1].y - cloud->points[end1].y)/2;
pose[2] = cloud->points[end1].z + (cloud->points[start1].z - cloud->points[end1].z)/2;
//define vector between sides of leg
Eigen::Vector3f across, along, rotvec, planevec, robvec;
across << cloud->points[end1].x - cloud->points[start1].x,
          cloud->points[end1].y - cloud->points[start1].y,
          cloud->points[end1].z - cloud->points[start1].z;
across = across.normalized();

//define vector that is the longest line
along << cloud->points[end2].x - cloud->points[start2].x,
         cloud->points[end2].y - cloud->points[start2].y,
         cloud->points[end2].z - cloud->points[start2].z;
along = along.normalized();

rotvec = across.cross(along); //vector to be rotated around the plane vector
rotvec = rotvec.normalized();
planevec = across.cross(rotvec); //vector to define plane
planevec = planevec.normalized();
robvec << pose[0], pose[1], pose[2]; // unit vector from the leg to the robot (or reverse that?)

float angle[36] = {}; //contains found angles. mb not necessary
float smallAngle = 360; //smallest found angle. high initial val
int vecIdx; //which vector fits best
float dotp; //dotproduct
for (int i = 0; i < 36; i++)
{ //test which vector fits best
    rotvec = rotvec*cos(10) + (planevec.cross(rotvec))*sin(10); //Rodrigue's formula, abridged
    dotp = rotvec.transpose() * robvec;
    angle[i] = acos(dotp) * 180/3.14159265; //angle in deg
    if (angle[i]<smallAngle)
    {
        smallAngle = angle[i];
        vecIdx = i;
    }
}
rotvec = rotvec*cos(vecIdx*10) + (planevec.cross(rotvec))*sin(vecIdx*10);
rotvec = rotvec.normalized();

pose[3] = acos(planevec(0));
pose[4] = asin(rotvec(1)); //in radians
pose[5] = atan2(rotvec(0),rotvec(2));
}
float* Get_values(){
    return pose;
}
};

float Dist(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int index1, int index2){
float distx = cloud->points[index1].x - cloud->points[index2].x;
float disty = cloud->points[index1].y - cloud->points[index2].y;
float distz = cloud->points[index1].z - cloud->points[index2].z;

float len = sqrt(pow(distx, 2.0) + pow(disty, 2.0) + pow(distz, 2.0));
return len;
}

float Dist(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int index1){ //to origin
float len = sqrt(pow(cloud->points[index1].x, 2.0) + pow(cloud->points[index1].y, 2.0) + pow(cloud->points[index1].z, 2.0));
return len;
}

tuple<int, int> eighteen (pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, int start, int end){
float distx = cloud->points[start].x - cloud->points[end].x;
float disty = cloud->points[start].y - cloud->points[end].y;
float distz = cloud->points[start].z - cloud->points[end].z;
//Finding the length of the line
float len1 = sqrt(pow(distx, 2.0) + pow(disty, 2.0) + pow(distz, 2.0));

float difflen = 0.14/len1;

//Scaling the line to 0.18
len1 *= difflen;

//By modifying t we can find any point on the line
//where t = 0 corresponds to our starting point and t = 1 to our end point
//any value of t in between will be a point on the line between start and end
float t = -1 * difflen;
//Here we calculate the direction "vector"
float directionx = cloud->points[start].x - cloud->points[end].x;
float directiony = cloud->points[start].y - cloud->points[end].y;
float directionz = cloud->points[start].z - cloud->points[end].z;

//Pushing the point on the line 18cm from start to the pointcloud
//Notice that each point is calculated by adding the start point with the direction vector multiplied by t
pcl::PointXYZ pp;
pp.x = cloud->points[start].x + directionx * t; pp.y = cloud->points[start].y + directiony * t; pp.z = cloud->points[start].z + directionz * t;
cloud->push_back(pp);

cout << cloud->points.size() << endl;
//Here we find the distance from the start of the line to the point
int lastindex = cloud->points.size()-1;
float distpointx = cloud->points[start].x - cloud->points[lastindex].x;
float distpointy = cloud->points[start].y - cloud->points[lastindex].y;
float distpointz = cloud->points[start].z - cloud->points[lastindex].z;
float lenToPoint = sqrt(pow(distpointx, 2.0) + pow(distpointy, 2.0) + pow(distpointz, 2.0));
float lenToPoint1 = lenToPoint;
//Here we search for the smallest distance to the 18cm point from any point in the pointclooud
//The shortst distance to the point will then be the last i value -1 since the smallest distance to the point is the distance from the point to the itself
int p1 = 0, p2 = 0;
vector<int> temp;
for(int i = 0; i < cloud->points.size(); i++)
{
    if (i!= cloud->points.size()-1)
    {
        float itepointx = cloud->points[i].x - cloud->points[lastindex].x;
        float itepointy = cloud->points[i].y - cloud->points[lastindex].y;
        float itepointz = cloud->points[i].z - cloud->points[lastindex].z;

        if(lenToPoint > sqrt(pow(itepointx, 2.0) + pow(itepointy, 2.0) + pow(itepointz, 2.0)))
        {
            lenToPoint = sqrt(pow(itepointx, 2.0) + pow(itepointy, 2.0) + pow(itepointz, 2.0));
            //temp.push_back(i);
            p1 = i;///temp.rbegin()[1];
        }
    } 
}
for(int i = 0; i < cloud->points.size(); i++)
{
    if (abs(p1-i)>30 && i!= cloud->points.size()-1)//exclude immediate neighbors
    {
        float itepointx = cloud->points[i].x - cloud->points[p1].x;
        float itepointy = cloud->points[i].y - cloud->points[p1].y;
        float itepointz = cloud->points[i].z - cloud->points[p1].z;

        if(lenToPoint1 > sqrt(pow(itepointx, 2.0) + pow(itepointy, 2.0) + pow(itepointz, 2.0)))
        {
            lenToPoint1 = sqrt(pow(itepointx, 2.0) + pow(itepointy, 2.0) + pow(itepointz, 2.0));
            p2 = i;
        }
    }
}
return make_tuple(p1, p2);
}

bool startIsNarrower(float myArray[], int arraySize){
//Checks if the first half of an array has a smaller mean value than the 2nd half
float sum = 0.0, avg1, avg2;
    for (int i = 0; i < arraySize/2; i++) //sum first half
    {
        sum += myArray[i];
    }
    avg1 = sum/arraySize/2; //mean of first half

    sum = 0.0;
    for (int i = arraySize/2; i < arraySize; i++)
    {
        sum += myArray[i];
    }
    avg2 = sum/arraySize/2;

    if (avg1<avg2) //compare
    {
        return true;
    } else {
        return false;
    }
}

int main (int argc, char** argv) {
srand (static_cast <unsigned> (time(0)));
// load point cloud
pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);

pcl::io::loadPCDFile ("concaveboi.pcd", *cloud); //prev: hulltest.pdc
cout << "Point cloud size: " << cloud->points.size() << endl;

///////////////////////////////////////////////////////////////////////////
// Try out different pairs, looking for the longest distance between them
float longest = 0.0; // longest line found
float len = 0.0;     // current length being evaluated
float xdist = 0.0;
float ydist = 0.0;   // distance between points in y-direction
float zdist = 0.0;
int idx1 = 0;        // one end of the line
int idx2 = 0;        // other end of the line

for (int i = 0; i < cloud->points.size()/2; i++)
{
    for (int j = 0; j < cloud->points.size(); j++)
    {
        if (abs(i-j)>cloud->points.size()/5) //exclude immediate neighbors
        {
            xdist = cloud->points[i].x - cloud->points[j].x;
            ydist = cloud->points[i].y - cloud->points[j].y;
            zdist = cloud->points[i].z - cloud->points[j].z;
            len = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));

            if (len>longest)
            {
                longest = len;
                idx1 = i;
                idx2 = j;
            }
        }
    }
}
cout << "The longest line found goes between points [" << idx1 << "," << idx2
     << "] and has length: " << longest << endl;

/////////////////////////////////////////////////////////////////////////////////
// Placing the cut based on: thickness increase. Index increment
const int gran = 1; //granularity, how many indices are skipped per jump
const int iterations = cloud->points.size()/(gran*2);
float shortest[iterations] = {1.0}; //distances between pairs of idx3[i] and idx4[i]
int idx3[iterations] = {};
int idx4[iterations] = {}; //nearest point opposite from an idx3

// populate idx3 with indices, starting from idx1
for (int i = 0; i < iterations; i++)
{
    if (idx1 + i*gran > cloud->points.size()-1)
    {// in case we would exceed size of point cloud
        idx3[i] = idx1 + i*gran - cloud->points.size();
    }
    else
    {
        idx3[i] = idx1 + i*gran;
    }
}
// find idx4 for each idx3
for (int i = 0; i < iterations; i++)
{
    if (abs(idx3[i] - idx1)<20 || idx3[i]<10 && abs(cloud->points.size() - idx1)<10)
    {//if idx3 is close to the tip
        if (idx3[i]>idx1) //if nearest point is to the right
        {
            if (((idx1-idx3[i])<10) || (cloud->points.size() - idx1)<10)
            {//if the tip is close to idx(0)
                  idx4[i] = cloud->points.size() - (idx3[i] - idx1);
            }
            else
            {
                  idx4[i] = idx1 - idx3[i];
            }
        }
        else //if nearest is to the left
        {
            if ((idx1 + idx3[i]) >= cloud->points.size()) //if the tip is close to idx(0)
            {
                idx4[i] = (idx1 - idx3[i]) - (cloud->points.size() - idx3[i]);
            }
            else
            {
                idx4[i] = idx1 + idx3[i];
            }
        }
        xdist = cloud->points[idx3[i]].x - cloud->points[idx4[i]].x;
        ydist = cloud->points[idx3[i]].y - cloud->points[idx4[i]].y;
        zdist = cloud->points[idx3[i]].z - cloud->points[idx4[i]].z;
        shortest[i] = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));
    }
    else //if idx3 is not close to the tip
    {
        shortest[i] = 1.0;
        for (int point = 0; point < cloud->points.size(); point++)
        {//search for nearest opposite
            if (abs(point-idx3[i])>30)//exclude immediate neighbors
            {
                xdist = cloud->points[idx3[i]].x - cloud->points[point].x;
                ydist = cloud->points[idx3[i]].y - cloud->points[point].y;
                zdist = cloud->points[idx3[i]].z - cloud->points[point].z;
                len = sqrt(pow(xdist, 2.0) + pow(ydist, 2.0) + pow(zdist, 2.0));

                if (len<shortest[i])
                {
                    shortest[i] = len;
                    idx4[i] = point;
                }
            }
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////
// Make sure that idx1 is the narrow end
if (startIsNarrower(shortest, iterations))
{
    cout << "idx1 is in the narrow end" << endl;
} else {
    int temp = idx1; // switch them
    idx1 = idx2;
    idx2 = temp;
    cout << "idx1 was in the wide end" << endl;
}
/////////////////////////////////////////////////////////////////////////////////
// Discriminate lines based on angle // Calculate angles
Eigen::Vector3f vec1, vec2;
vec1 << cloud->points[idx2].x - cloud->points[idx1].x,
        cloud->points[idx2].y - cloud->points[idx1].y,
        cloud->points[idx2].z - cloud->points[idx1].z;
vec1 = vec1.normalized(); // vector version of the longest line (green)

std::vector<int> valVec; //only valid lines 
uint valCount = 0; //How many lines are valid
float angle[iterations] = {}; //Angles of the lines
for (int i = 0; i < iterations; i++)
{
    vec2 << cloud->points[idx3[i]].x - cloud->points[idx4[i]].x,
            cloud->points[idx3[i]].y - cloud->points[idx4[i]].y,
            cloud->points[idx3[i]].z - cloud->points[idx4[i]].z;
    vec2 = vec2.normalized(); //to ensure it has length 1
    
    float dotp = vec1.transpose() * vec2;
    angle[i] = acos(dotp) * 180/3.14159265; //angle in deg

    if (abs(angle[i] - 90) < 35) //accept a diff of up to x deg // WEIRD ERROR WITH LOW VALUE
    {
        valVec.push_back(i+1); //positive indicates valid line
        valCount++; // count valid lines
    }
    else
    {
        valVec.push_back(0); // 0 indicates invalid line
    }
}
//Make new array with only valid lines
float valShort[valCount] = {}; //lengths of valid lines
uint relcount = 0;
for (int i = 0; i < iterations; i++)
{
    if (valVec[i]) //if this index's line is valid
    {
        valShort[relcount] = shortest[i];
        relcount++;
    }
}
///////////////////////////////////////////////////////////////////////////////
// Evaluate thickness // Find local minimum, starting from the middle
uint minIdx = valCount/2; //Where we start searching
bool minFound = false; //has the best local minimum been found?
uint minTries = 0; //times we have hit a local minimum
uint check = valCount/12; //Check a nr of steps, proportional to size of array
int dir = 1; //is pos to ensure that it can move, if it starts in a local minimum
while(!minFound)
{
    if (valShort[minIdx]>valShort[minIdx-1])
    {
        minIdx--;
        dir = -1;
        cout << "minus" << endl;
    }
    else if (valShort[minIdx]>valShort[minIdx+1])
    {
        minIdx++;
        dir = 1;
        cout << "plus" << endl;
    }
    else if (valShort[minIdx]<valShort[minIdx+1] && valShort[minIdx]<valShort[minIdx-1])
    {
        cout << "Possible min at: " << minIdx << endl;
        for (int i = 1; i <= check; i++)
        {
            if (valShort[minIdx + i*dir] < valShort[minIdx])
            {
                minIdx = minIdx + i*dir;
                i = check;
            }
        }
        if (!dir)
        {
            minFound = true;
            cout << "Found min: [" << minIdx << "] with value: [" << valShort[minIdx] << "]" << endl;
        }
        if (minTries < 1) //if we haven't hit a local minimum before
        {
            dir = -dir; //swap direction
            minTries++;
        }
        else
        {
            dir = 0;
        }
    }
    else 
    {
        cout << "Error in finding minimum!" << endl;
        break;
    }
}
// Go from minIdx back to real idx of local minimum
uint finIdx = 0;
for (int i = 0; i < iterations; i++)
{
    if (valVec[i])
    {
        finIdx++;
    }
    if (finIdx == minIdx + 1)
    {
        finIdx = i*gran;
        i = iterations;
    }
}
cout << "Min has original idx: [" << finIdx << "] with value: [" << shortest[finIdx] << "]" << endl;

////////////////////////////////////////////////////////////////////////////////
// Placing the cut based on: distance from tip, 0.18m
int point1, point2; //coordinates where the cut should be
tie(point1, point2) = eighteen(cloud, idx1, idx2);
int lastindex = cloud->points.size()-1;

/////////////////////////////////////////////////////////////////////////////////
// Combine the two estimates
if (abs(finIdx-point1)<cloud->points.size()/40 || abs(finIdx-point2)<cloud->points.size()/40)
{// the two estimates are close
    point1 = idx4[finIdx];
    point2 = idx3[finIdx];
}
///////////////////////////////////////////////////////////////////////////////
// Output concluded end effector pose
Pose myPose;
myPose.Set_values(cloud, point1, point2, idx1, idx2);
float *pose1 = myPose.Get_values();

cout << pose1[0] << endl;
cout << pose1[1] << endl;
cout << pose1[2] << endl;
cout << pose1[3] << endl;
cout << pose1[4] << endl;
cout << pose1[5] << endl;

///////////////////////////////////////////////////////////////////////////////
// Viewer //
pcl::visualization::PCLVisualizer viewer("PCL Viewer");
viewer.setBackgroundColor (0, 0, 0);
viewer.addPointCloud<pcl::PointXYZ> (cloud, "sample cloud");

for(int j = 0; j < iterations; j++) //idx-wise lines across the leg
{
    stringstream ss;
    ss << j;
    string str = ss.str();
    if (valVec[j])
    {
        if (j == finIdx) //concluded index for thickness evaluation
        {
            viewer.addLine(cloud->points[idx3[j]], cloud->points[idx4[j]], 1, 1, 0, str); //concluded line
        }
        else
        {
            viewer.addLine(cloud->points[idx3[j]], cloud->points[idx4[j]], 1, 0, 0, str); //between points
        }
    }
}
viewer.addLine(cloud->points[idx1], cloud->points[idx2], 0, 1, 0, "q"); //Show longest line
viewer.addLine(cloud->points[point1], cloud->points[point2], 1, 1, 1, "t"); //18cm line

while(!viewer.wasStopped ())
{
    viewer.spinOnce ();
}
return 0;
}
