#include "normal_include.h"
#include "g2o_optim.h"

float inner_cx = 160.5912;
float inner_cy = 120.4792;
float inner_fx = 253.0589;
float inner_fy = 254.1649;

Eigen::Vector3f get3DPoint(cv::Point2f imgpos,cv::Mat& dimage);

int main(int argc,char** argv)
{
    cv::Mat Image1,Image2,dImage1,dImage2;
    std::string reading_data_num1,reading_data_num2;
    
    if(argc < 2)
    {
        std::cout << "Need two argument for data to be matched" << std::endl;
        return -1;
    }

    reading_data_num1 = argv[1];
    reading_data_num2 = argv[2];

    Image1 = cv::imread("../savings/rgb/rgb"+ reading_data_num1 + ".jpg");
    dImage1 = cv::imread("../savings/depth/depth"+ reading_data_num1 + ".jpg");
    Image2 = cv::imread("../savings/rgb/rgb"+ reading_data_num2 + ".jpg");
    dImage2 = cv::imread("../savings/depth/depth"+ reading_data_num2 + ".jpg");


//feature fingding and matching (same as run_feature_match.cpp)

    cv::Ptr<cv::FeatureDetector> fastdetect;
    cv::Ptr<cv::DescriptorExtractor> briefext;
    cv::Ptr<cv::DescriptorMatcher> bfmatcher;
    std::vector<cv::DMatch> matchepoints;
    std::vector<cv::DMatch> goodmatchepoints;
    fastdetect = cv::FastFeatureDetector::create(50);
    briefext = cv::ORB::create();
    bfmatcher = cv::DescriptorMatcher::create("BruteForce-Hamming");

    cv::Mat grayImage1;
    std::vector<cv::KeyPoint> featurepoints1;
    cv::Mat briefdesc1;
    std::vector<cv::DMatch> goodmatchepoints1;
    cv::cvtColor(Image1, grayImage1, cv::COLOR_BGR2GRAY);
    fastdetect->detect(grayImage1, featurepoints1);
    briefext->compute(grayImage1, featurepoints1, briefdesc1);
    
    cv::Mat grayImage2;
    std::vector<cv::KeyPoint> featurepoints2;
    cv::Mat briefdesc2;
    std::vector<cv::DMatch> goodmatchepoints2;
    cv::cvtColor(Image2, grayImage2, cv::COLOR_BGR2GRAY);
    fastdetect->detect(grayImage2, featurepoints2);
    briefext->compute(grayImage2, featurepoints2, briefdesc2);


   if(!briefdesc1.empty() && !briefdesc2.empty())
    {
        bfmatcher->match(briefdesc1, briefdesc2, matchepoints);
    }
    else
    {
        std::cout << "find feature failed" << std::endl;
        return -1;
    }
    if(!matchepoints.empty())
    {
        auto min_max = std::minmax_element(matchepoints.begin(), matchepoints.end(),
                                [](const cv::DMatch &m1, const cv::DMatch &m2) { return m1.distance < m2.distance; });
        double min_dist = min_max.first->distance;
        for(ushort i = 0; i < matchepoints.size(); i++)
        {
            if (matchepoints[i].distance <= std::max(1.5 * min_dist, 30.0))
            {
                goodmatchepoints.push_back(matchepoints[i]);
            }
        }
    }
    else
    {
        std::cout << "match failed" << std::endl;
        return -2;
    }

    std::cout << "Found " << goodmatchepoints.size() << " matched points" << std::endl;
    cv::Mat img_match;
    cv::namedWindow("img_match", cv::WINDOW_NORMAL);
    cv::drawMatches(Image1, featurepoints1, Image2, featurepoints2, goodmatchepoints, img_match);
    cv::imshow("img_match", img_match);
    
//g2o optimization
    Eigen::Matrix4f pose;
    SE3 pose_estimate;

    typedef g2o::BlockSolver_6_3 BlockSolverType;
    typedef g2o::LinearSolverDense<BlockSolverType::PoseMatrixType> LinearSolverType;

    auto solver = new g2o::OptimizationAlgorithmLevenberg(g2o::make_unique<BlockSolverType>(g2o::make_unique<LinearSolverType>()));
    g2o::SparseOptimizer optimizer;
    optimizer.setAlgorithm(solver);
    optimizer.setVerbose(true);

    VertexPose *vertex_pose = new VertexPose();
    vertex_pose->setEstimate(pose_estimate);
    vertex_pose->setId(0);
    optimizer.addVertex(vertex_pose);

    for(ushort i = 0; i < goodmatchepoints.size(); i++)
    {
        EdgeProjectionPoseOnly *edge = new EdgeProjectionPoseOnly(get3DPoint(featurepoints2[goodmatchepoints[i].trainIdx].pt,dImage2).cast<double>());
        edge->setId(i);
        edge->setVertex(0, vertex_pose);
        edge->setMeasurement(get3DPoint(featurepoints1[goodmatchepoints[i].queryIdx].pt,dImage1).cast<double>());
        edge->setInformation(Eigen::Matrix3d::Identity());
        edge->setRobustKernel(new g2o::RobustKernelHuber);
        optimizer.addEdge(edge);
    }

    optimizer.initializeOptimization();
    optimizer.optimize(30);
    
    pose = vertex_pose->estimate().matrix().cast<float>();
    std::cout << pose << std::endl;

    cv::waitKey(0);

    return 0;
}


Eigen::Vector3f get3DPoint(cv::Point2f imgpos,cv::Mat& dimage)
{
    Eigen::Matrix<float, 3, 1> temppoint;
    float depth;

    depth = dimage.at<ushort>(imgpos.x,imgpos.y);
    temppoint << (imgpos.x - inner_cx)*depth/inner_fx, (imgpos.y - inner_cy)*depth/inner_fy, depth;

    return temppoint;
}

