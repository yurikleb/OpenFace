///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, Carnegie Mellon University and University of Cambridge,
// all rights reserved.
//
// ACADEMIC OR NON-PROFIT ORGANIZATION NONCOMMERCIAL RESEARCH USE ONLY
//
// BY USING OR DOWNLOADING THE SOFTWARE, YOU ARE AGREEING TO THE TERMS OF THIS LICENSE AGREEMENT.  
// IF YOU DO NOT AGREE WITH THESE TERMS, YOU MAY NOT USE OR DOWNLOAD THE SOFTWARE.
//
// License can be found in OpenFace-license.txt

//     * Any publications arising from the use of this software, including but
//       not limited to academic journal and conference publications, technical
//       reports and manuals, must cite at least one of the following works:
//
//       OpenFace: an open source facial behavior analysis toolkit
//       Tadas Baltrušaitis, Peter Robinson, and Louis-Philippe Morency
//       in IEEE Winter Conference on Applications of Computer Vision, 2016  
//
//       Rendering of Eyes for Eye-Shape Registration and Gaze Estimation
//       Erroll Wood, Tadas Baltrušaitis, Xucong Zhang, Yusuke Sugano, Peter Robinson, and Andreas Bulling 
//       in IEEE International. Conference on Computer Vision (ICCV),  2015 
//
//       Cross-dataset learning and person-speci?c normalisation for automatic Action Unit detection
//       Tadas Baltrušaitis, Marwa Mahmoud, and Peter Robinson 
//       in Facial Expression Recognition and Analysis Challenge, 
//       IEEE International Conference on Automatic Face and Gesture Recognition, 2015 
//
//       Constrained Local Neural Fields for robust facial landmark detection in the wild.
//       Tadas Baltrušaitis, Peter Robinson, and Louis-Philippe Morency. 
//       in IEEE Int. Conference on Computer Vision Workshops, 300 Faces in-the-Wild Challenge, 2013.    
//
///////////////////////////////////////////////////////////////////////////////
// FaceTrackingVid.cpp : Defines the entry point for the console application for tracking faces in videos.

// Libraries for landmark detection (includes CLNF and CLM modules)
#include "LandmarkCoreIncludes.h"
#include "GazeEstimation.h"
#include <Face_utils.h>
#include <FaceAnalyser.h>


#include <fstream>
#include <sstream>

// OpenCV includes
#include <opencv2/videoio/videoio.hpp>  // Video write
#include <opencv2/videoio/videoio_c.h>  // Video write
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// Boost includes
#include <filesystem.hpp>
#include <filesystem/fstream.hpp>

//OSC Includes
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

//OSC Settings
#define ADDRESS "127.0.0.1"
#define PORT 6448
#define OUTPUT_BUFFER_SIZE 2500

UdpTransmitSocket transmitSocket(IpEndpointName(ADDRESS, PORT));

char buffer[OUTPUT_BUFFER_SIZE];
osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

#define INFO_STREAM( stream ) \
std::cout << stream << std::endl

#define WARN_STREAM( stream ) \
std::cout << "Warning: " << stream << std::endl

#define ERROR_STREAM( stream ) \
std::cout << "Error: " << stream << std::endl

static void printErrorAndAbort( const std::string & error )
{
    std::cout << error << std::endl;
    abort();
}

#define FATAL_STREAM( stream ) \
printErrorAndAbort( std::string( "Fatal error: " ) + stream )

using namespace std;
using namespace boost::filesystem;

vector<string> get_arguments(int argc, char **argv)
{

	vector<string> arguments;

	for(int i = 0; i < argc; ++i)
	{
		arguments.push_back(string(argv[i]));
	}
	return arguments;
}

// Some globals for tracking timing information for visualisation
double fps_tracker = -1.0;
int64 t0 = 0;

// Visualising the results
void visualise_tracking(cv::Mat& captured_image, cv::Mat_<float>& depth_image, const LandmarkDetector::CLNF& face_model, const LandmarkDetector::FaceModelParameters& det_parameters, cv::Point3f gazeDirection0, cv::Point3f gazeDirection1, int frame_count, double fx, double fy, double cx, double cy)
{

	// Drawing the facial landmarks on the face and the bounding box around it if tracking is successful and initialised
	double detection_certainty = face_model.detection_certainty;
	bool detection_success = face_model.detection_success;

	double visualisation_boundary = 0.2;

	// Only draw + send data over OSC if the reliability is reasonable, the value is slightly ad-hoc
	if (detection_certainty < visualisation_boundary)
	{
		
		//Draw Face Landmarks
		LandmarkDetector::Draw(captured_image, face_model);

		double vis_certainty = detection_certainty;
		if (vis_certainty > 1)
			vis_certainty = 1;
		if (vis_certainty < -1)
			vis_certainty = -1;

		vis_certainty = (vis_certainty + 1) / (visualisation_boundary + 1);

		// A rough heuristic for box around the face width
		int thickness = (int)std::ceil(2.0* ((double)captured_image.cols) / 640.0);

		cv::Vec6d pose_estimate_to_draw = LandmarkDetector::GetCorrectedPoseWorld(face_model, fx, fy, cx, cy);

		// Draw it in reddish if uncertain, blueish if certain
		LandmarkDetector::DrawBox(captured_image, pose_estimate_to_draw, cv::Scalar((1 - vis_certainty)*255.0, 0, vis_certainty * 255), thickness, fx, fy, cx, cy);
		
		//Draw Gaze
		if (det_parameters.track_gaze && detection_success && face_model.eye_model)
		{
			FaceAnalysis::DrawGaze(captured_image, face_model, gazeDirection0, gazeDirection1, fx, fy, cx, cy);
		}

		//Send data via OSC ( 3 gaze coords + 136 landmarks = 142 data points) 
		p.Clear();
		
		//Gaze vectors
		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/openFace/allData");
			//<< gazeDirection0.x << gazeDirection0.y << gazeDirection0.z
			//<< gazeDirection1.x << gazeDirection1.y << gazeDirection1.z;

		//Head pose vector
		//for (int i = 0; i < 6; i++) {
		//	p << (float)pose_estimate_to_draw[i];
		//}

		//Camera Data
		//p << (float)fx << (float)fy << (float)cx << (float)cy;

		//Face landmarks
		for (int i = 0; i < face_model.detected_landmarks.rows; i++) {
			p << (float)face_model.detected_landmarks[i][0];
		}

		p << osc::EndMessage
			<< osc::EndBundle;

		transmitSocket.Send(p.Data(), p.Size());

	}

	// Work out the framerate
	if (frame_count % 10 == 0)
	{
		double t1 = cv::getTickCount();
		fps_tracker = 10.0 / (double(t1 - t0) / cv::getTickFrequency());
		t0 = t1;
	}

	// Write out the framerate on the image before displaying it
	char fpsC[255];
	std::sprintf(fpsC, "%d", (int)fps_tracker);
	string fpsSt("FPS:");
	fpsSt += fpsC;
	cv::putText(captured_image, fpsSt, cv::Point(10, 20), CV_FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(255, 0, 0));

	if (!det_parameters.quiet_mode)
	{
		cv::namedWindow("tracking_result", 1);
		cv::imshow("tracking_result", captured_image);

		if (!depth_image.empty())
		{
			// Division needed for visualisation purposes
			imshow("depth", depth_image / 2000.0);
		}

	}
}

// Useful utility for creating directories for storing the output files
void create_directory_from_file(string output_path)
{

	// Creating the right directory structure
	
	// First get rid of the file
	auto p = path(path(output_path).parent_path());

	if(!p.empty() && !boost::filesystem::exists(p))		
	{
		bool success = boost::filesystem::create_directories(p);
		if(!success)
		{
			cout << "Failed to create a directory... " << p.string() << endl;
		}
	}
}

void create_directory(string output_path)
{

	// Creating the right directory structure
	auto p = path(output_path);

	if(!boost::filesystem::exists(p))		
	{
		bool success = boost::filesystem::create_directories(p);
		
		if(!success)
		{
			cout << "Failed to create a directory..." << p.string() << endl;
		}
	}
}

// Output all of the information into one file in one go (quite a few parameters, but simplifies the flow)
void outputAllFeatures(bool output_2D_landmarks, bool output_3D_landmarks,
	bool output_model_params, bool output_pose, bool output_AUs, bool output_gaze,
	const LandmarkDetector::CLNF& face_model, int frame_count, double time_stamp, bool detection_success,
	cv::Point3f gazeDirection0, cv::Point3f gazeDirection1, const cv::Vec6d& pose_estimate, double fx, double fy, double cx, double cy,
	const FaceAnalysis::FaceAnalyser& face_analyser)
{

	double confidence = 0.5 * (1 - face_model.detection_certainty);

	// *output_file << frame_count + 1 << ", " << time_stamp << ", " << confidence << ", " << detection_success;

	// Output the estimated gaze
	// if (output_gaze)
	// {
	// 	*output_file << ", " << gazeDirection0.x << ", " << gazeDirection0.y << ", " << gazeDirection0.z
	// 		<< ", " << gazeDirection1.x << ", " << gazeDirection1.y << ", " << gazeDirection1.z;
	// }

	// Output the estimated head pose
	// if (output_pose)
	// {
	// 	if(face_model.tracking_initialised)
	// 	{
	// 		*output_file << ", " << pose_estimate[0] << ", " << pose_estimate[1] << ", " << pose_estimate[2]
	// 			<< ", " << pose_estimate[3] << ", " << pose_estimate[4] << ", " << pose_estimate[5];
	// 	}
	// 	else
	// 	{
	// 		*output_file << ", 0, 0, 0, 0, 0, 0";
	// 	}
	// }

	// Output the detected 2D facial landmarks
	// if (output_2D_landmarks)
	// {
	// 	for (int i = 0; i < face_model.pdm.NumberOfPoints() * 2; ++i)
	// 	{
	// 		if(face_model.tracking_initialised)
	// 		{
	// 			*output_file << ", " << face_model.detected_landmarks.at<double>(i);
	// 		}
	// 		else
	// 		{
	// 			*output_file << ", 0";
	// 		}
	// 	}
	// }

	// Output the detected 3D facial landmarks
	// if (output_3D_landmarks)
	// {
	// 	cv::Mat_<double> shape_3D = face_model.GetShape(fx, fy, cx, cy);
	// 	for (int i = 0; i < face_model.pdm.NumberOfPoints() * 3; ++i)
	// 	{
	// 		if (face_model.tracking_initialised)
	// 		{
	// 			*output_file << ", " << shape_3D.at<double>(i);
	// 		}
	// 		else
	// 		{
	// 			*output_file << ", 0";
	// 		}
	// 	}
	// }

	// if (output_model_params)
	// {
	// 	for (int i = 0; i < 6; ++i)
	// 	{
	// 		if (face_model.tracking_initialised)
	// 		{
	// 			*output_file << ", " << face_model.params_global[i];
	// 		}
	// 		else
	// 		{
	// 			*output_file << ", 0";
	// 		}
	// 	}
	// 	for (int i = 0; i < face_model.pdm.NumberOfModes(); ++i)
	// 	{
	// 		if(face_model.tracking_initialised)
	// 		{
	// 			*output_file << ", " << face_model.params_local.at<double>(i, 0);
	// 		}
	// 		else
	// 		{
	// 			*output_file << ", 0";
	// 		}
	// 	}
	// }



	if (output_AUs)
	{
		auto aus_reg = face_analyser.GetCurrentAUsReg();

		vector<string> au_reg_names = face_analyser.GetAURegNames();
		std::sort(au_reg_names.begin(), au_reg_names.end());

		// write out ar the correct index
		for (string au_name : au_reg_names)
		{
			for (auto au_reg : aus_reg)
			{
				if (au_name.compare(au_reg.first) == 0)
				{
					// *output_file << ", " << au_reg.second;
					cout << " au_name : " << au_name << " = "  << au_reg.second << endl;
					break;
				}
			}
		}

		if (aus_reg.size() == 0)
		{
			for (size_t p = 0; p < face_analyser.GetAURegNames().size(); ++p)
			{
				// *output_file << ", 0";
			}
		}

		auto aus_class = face_analyser.GetCurrentAUsClass();

		vector<string> au_class_names = face_analyser.GetAUClassNames();
		std::sort(au_class_names.begin(), au_class_names.end());

		// write out ar the correct index
		for (string au_name : au_class_names)
		{
			for (auto au_class : aus_class)
			{
				if (au_name.compare(au_class.first) == 0)
				{
					// *output_file << ", " << au_class.second;
					cout << " au_name : " << au_name << " = " << au_class.second << endl; 
					break;
				}
			}
		}

		if (aus_class.size() == 0)
		{
			for (size_t p = 0; p < face_analyser.GetAUClassNames().size(); ++p)
			{
				// *output_file << ", 0";
			}
		}
	}
	// *output_file << endl;
}

void get_output_feature_params(vector<string> &output_similarity_aligned, vector<string> &output_hog_aligned_files, double &similarity_scale,
	int &similarity_size, bool &grayscale, bool& verbose, bool& dynamic,
	bool &output_2D_landmarks, bool &output_3D_landmarks, bool &output_model_params, bool &output_pose, bool &output_AUs, bool &output_gaze,
	vector<string> &arguments)
{
	output_similarity_aligned.clear();
	output_hog_aligned_files.clear();

	bool* valid = new bool[arguments.size()];

	for (size_t i = 0; i < arguments.size(); ++i)
	{
		valid[i] = true;
	}

	string output_root = "";

	// By default the model is dynamic
	dynamic = true;

	string separator = string(1, boost::filesystem::path::preferred_separator);

	// First check if there is a root argument (so that videos and outputs could be defined more easilly)
	for (size_t i = 0; i < arguments.size(); ++i)
	{
		if (arguments[i].compare("-root") == 0)
		{
			output_root = arguments[i + 1] + separator;
			i++;
		}
		if (arguments[i].compare("-outroot") == 0)
		{
			output_root = arguments[i + 1] + separator;
			i++;
		}
	}

	for (size_t i = 0; i < arguments.size(); ++i)
	{
		if (arguments[i].compare("-simalign") == 0)
		{
			output_similarity_aligned.push_back(output_root + arguments[i + 1]);
			create_directory(output_root + arguments[i + 1]);
			valid[i] = false;
			valid[i + 1] = false;
			i++;
		}
		else if (arguments[i].compare("-hogalign") == 0)
		{
			output_hog_aligned_files.push_back(output_root + arguments[i + 1]);
			create_directory_from_file(output_root + arguments[i + 1]);
			valid[i] = false;
			valid[i + 1] = false;
			i++;
		}
		else if (arguments[i].compare("-verbose") == 0)
		{
			verbose = true;
		}
		else if (arguments[i].compare("-au_static") == 0)
		{
			dynamic = false;
		}
		else if (arguments[i].compare("-g") == 0)
		{
			grayscale = true;
			valid[i] = false;
		}
		else if (arguments[i].compare("-simscale") == 0)
		{
			similarity_scale = stod(arguments[i + 1]);
			valid[i] = false;
			valid[i + 1] = false;
			i++;
		}
		else if (arguments[i].compare("-simsize") == 0)
		{
			similarity_size = stoi(arguments[i + 1]);
			valid[i] = false;
			valid[i + 1] = false;
			i++;
		}
		else if (arguments[i].compare("-no2Dfp") == 0)
		{
			output_2D_landmarks = false;
			valid[i] = false;
		}
		else if (arguments[i].compare("-no3Dfp") == 0)
		{
			output_3D_landmarks = false;
			valid[i] = false;
		}
		else if (arguments[i].compare("-noMparams") == 0)
		{
			output_model_params = false;
			valid[i] = false;
		}
		else if (arguments[i].compare("-noPose") == 0)
		{
			output_pose = false;
			valid[i] = false;
		}
		else if (arguments[i].compare("-noAUs") == 0)
		{
			output_AUs = false;
			valid[i] = false;
		}
		else if (arguments[i].compare("-noGaze") == 0)
		{
			output_gaze = false;
			valid[i] = false;
		}
	}

	for (int i = arguments.size() - 1; i >= 0; --i)
	{
		if (!valid[i])
		{
			arguments.erase(arguments.begin() + i);
		}
	}

}

int main (int argc, char **argv)
{
	//OSC INIT
	(void)argc; // suppress unused parameter warnings
	(void)argv; // suppress unused parameter warnings
	INFO_STREAM("OSC Client Online!");

	vector<string> arguments = get_arguments(argc, argv);

	// Some initial parameters that can be overriden from command line	
	vector<string> files, depth_directories, output_video_files, out_dummy;
	
	// By default try webcam 0
	int device = 0;

	LandmarkDetector::FaceModelParameters det_parameters(arguments);

	// Get the input output file parameters
	
	// Indicates that rotation should be with respect to world or camera coordinates
	bool u;
	string output_codec;
	LandmarkDetector::get_video_input_output_params(files, depth_directories, out_dummy, output_video_files, u, output_codec, arguments);
	
	// The modules that are being used for tracking
	LandmarkDetector::CLNF clnf_model(det_parameters.model_location);	

	// Grab camera parameters, if they are not defined (approximate values will be used)
	float fx = 0, fy = 0, cx = 0, cy = 0;
	// Get camera parameters
	LandmarkDetector::get_camera_params(device, fx, fy, cx, cy, arguments);

	// If cx (optical axis centre) is undefined will use the image size/2 as an estimate
	bool cx_undefined = false;
	bool fx_undefined = false;
	if (cx == 0 || cy == 0)
	{
		cx_undefined = true;
	}
	if (fx == 0 || fy == 0)
	{
		fx_undefined = true;
	}

	// If multiple video files are tracked, use this to indicate if we are done
	bool done = false;	
	// int f_n = -1;
	
	det_parameters.track_gaze = true;

	//AU setup
	bool verbose = true;
	boost::filesystem::path config_path = boost::filesystem::path(".");
	boost::filesystem::path parent_path = boost::filesystem::path(".").parent_path();

	vector<string> output_similarity_align;
	vector<string> output_hog_align_files;

	double sim_scale = -1;
	int sim_size = 112;
	bool grayscale = false;
	bool video_output = false;
	bool dynamic = true; // Indicates if a dynamic AU model should be used (dynamic is useful if the video is long enough to include neutral expressions)
	int num_hog_rows;
	int num_hog_cols;

	// By default output all parameters, but these can be turned off to get smaller files or slightly faster processing times
	// use -no2Dfp, -no3Dfp, -noMparams, -noPose, -noAUs, -noGaze to turn them off
	bool output_2D_landmarks = true;
	bool output_3D_landmarks = true;
	bool output_model_params = true;
	bool output_pose = true;
	bool output_AUs = true;
	bool output_gaze = true;

	get_output_feature_params(output_similarity_align, output_hog_align_files, sim_scale, sim_size, grayscale, verbose, dynamic,
		output_2D_landmarks, output_3D_landmarks, output_model_params, output_pose, output_AUs, output_gaze, arguments);

	// Used for image masking
	string tri_loc;
	boost::filesystem::path tri_loc_path = boost::filesystem::path("model/tris_68_full.txt");
	if (boost::filesystem::exists(tri_loc_path))
	{
		tri_loc = tri_loc_path.string();
	}
	else if (boost::filesystem::exists(parent_path/tri_loc_path))
	{
		tri_loc = (parent_path/tri_loc_path).string();
	}
	else if (boost::filesystem::exists(config_path/tri_loc_path))
	{
		tri_loc = (config_path/tri_loc_path).string();
	}
	else
	{
		cout << "Can't find triangulation files, exiting" << endl;
		return 1;
	}

	// Will warp to scaled mean shape
	// cv::Mat_<double> similarity_normalised_shape = face_model.pdm.mean_shape * sim_scale;
	// // Discard the z component
	// similarity_normalised_shape = similarity_normalised_shape(cv::Rect(0, 0, 1, 2*similarity_normalised_shape.rows/3)).clone();

	// If multiple video files are tracked, use this to indicate if we are done
	// bool done = false;	
	int f_n = -1;
	int curr_img = -1;

	string au_loc;

	string au_loc_local;
	if (dynamic)
	{
		au_loc_local = "AU_predictors/AU_all_best.txt";
	}
	else
	{
		au_loc_local = "AU_predictors/AU_all_static.txt";
	}

	boost::filesystem::path au_loc_path = boost::filesystem::path(au_loc_local);
	if (boost::filesystem::exists(au_loc_path))
	{
		au_loc = au_loc_path.string();
	}
	else if (boost::filesystem::exists(parent_path/au_loc_path))
	{
		au_loc = (parent_path/au_loc_path).string();
	}
	else if (boost::filesystem::exists(config_path/au_loc_path))
	{
		au_loc = (config_path/au_loc_path).string();
	}
	else
	{
		cout << "Can't find AU prediction files, exiting" << endl;
		return 1;
	}

	// Creating a  face analyser that will be used for AU extraction
	// Make sure sim_scale is proportional to sim_size if not set
	if (sim_scale == -1) sim_scale = sim_size * (0.7 / 112.0);

	FaceAnalysis::FaceAnalyser face_analyser(vector<cv::Vec3d>(), sim_scale, sim_size, sim_size, au_loc, tri_loc);

	while(!done) // this is not a for loop as we might also be reading from a webcam
	{
		
		string current_file;

		// We might specify multiple video files as arguments
		if(files.size() > 0)
		{
			f_n++;			
		    current_file = files[f_n];
		}
		else
		{
			// If we want to write out from webcam
			f_n = 0;
		}
		
		bool use_depth = !depth_directories.empty();	

		// Do some grabbing
		cv::VideoCapture video_capture;
		if( current_file.size() > 0 )
		{
			if (!boost::filesystem::exists(current_file))
			{
				FATAL_STREAM("File does not exist");
				return 1;
			}

			current_file = boost::filesystem::path(current_file).generic_string();

			INFO_STREAM( "Attempting to read from file: " << current_file );
			video_capture = cv::VideoCapture( current_file );
		}
		else
		{
			INFO_STREAM( "Attempting to capture from device: " << device );
			video_capture = cv::VideoCapture( device );

			// Read a first frame often empty in camera
			cv::Mat captured_image;
			video_capture >> captured_image;
		}

		if (!video_capture.isOpened())
		{
			FATAL_STREAM("Failed to open video source");
			return 1;
		}
		else INFO_STREAM( "Device or file opened");

		cv::Mat captured_image;
		video_capture >> captured_image;		

		// If optical centers are not defined just use center of image
		if (cx_undefined)
		{
			cx = captured_image.cols / 2.0f;
			cy = captured_image.rows / 2.0f;
		}
		// Use a rough guess-timate of focal length
		if (fx_undefined)
		{
			fx = 500 * (captured_image.cols / 640.0);
			fy = 500 * (captured_image.rows / 480.0);

			fx = (fx + fy) / 2.0;
			fy = fx;
		}		
	
		int frame_count = 0;
		
		// saving the videos
		cv::VideoWriter writerFace;
		if (!output_video_files.empty())
		{
			try
 			{
				writerFace = cv::VideoWriter(output_video_files[f_n], CV_FOURCC(output_codec[0], output_codec[1], output_codec[2], output_codec[3]), 30, captured_image.size(), true);
			}
			catch(cv::Exception e)
			{
				WARN_STREAM( "Could not open VideoWriter, OUTPUT FILE WILL NOT BE WRITTEN. Currently using codec " << output_codec << ", try using an other one (-oc option)");
			}
		}

		// Use for timestamping if using a webcam
		int64 t_initial = cv::getTickCount();

		// Timestamp in seconds of current processing
		double time_stamp = 0;

		INFO_STREAM( "Starting tracking");
		while(!captured_image.empty())
		{		

			// Reading the images
			cv::Mat_<float> depth_image;
			cv::Mat_<uchar> grayscale_image;

			if(captured_image.channels() == 3)
			{
				cv::cvtColor(captured_image, grayscale_image, CV_BGR2GRAY);				
			}
			else
			{
				grayscale_image = captured_image.clone();				
			}
		
			// Get depth image
			if(use_depth)
			{
				char* dst = new char[100];
				std::stringstream sstream;

				sstream << depth_directories[f_n] << "\\depth%05d.png";
				sprintf(dst, sstream.str().c_str(), frame_count + 1);
				// Reading in 16-bit png image representing depth
				cv::Mat_<short> depth_image_16_bit = cv::imread(string(dst), -1);

				// Convert to a floating point depth image
				if(!depth_image_16_bit.empty())
				{
					depth_image_16_bit.convertTo(depth_image, CV_32F);
				}
				else
				{
					WARN_STREAM( "Can't find depth image" );
				}
			}
			
			// The actual facial landmark detection / tracking
			bool detection_success = LandmarkDetector::DetectLandmarksInVideo(grayscale_image, depth_image, clnf_model, det_parameters);
			
			// Visualising the results
			// Drawing the facial landmarks on the face and the bounding box around it if tracking is successful and initialised
			double detection_certainty = clnf_model.detection_certainty;

			// Gaze tracking, absolute gaze direction
			cv::Point3f gazeDirection0(0, 0, -1);
			cv::Point3f gazeDirection1(0, 0, -1);

			if (det_parameters.track_gaze && detection_success && clnf_model.eye_model)
			{
				FaceAnalysis::EstimateGaze(clnf_model, gazeDirection0, fx, fy, cx, cy, true);
				FaceAnalysis::EstimateGaze(clnf_model, gazeDirection1, fx, fy, cx, cy, false);
			}

			visualise_tracking(captured_image, depth_image, clnf_model, det_parameters, gazeDirection0, gazeDirection1, frame_count, fx, fy, cx, cy);
			
			// output the tracked video
			if (!output_video_files.empty())
			{
				writerFace << captured_image;
			}


			video_capture >> captured_image;

			cv::Mat sim_warped_img;
			face_analyser.AddNextFrame(captured_image, clnf_model, time_stamp, false, !det_parameters.quiet_mode);
			face_analyser.GetLatestAlignedFace(sim_warped_img);

			cv::Vec6d pose_estimate = LandmarkDetector::GetCorrectedPoseWorld(clnf_model, fx, fy, cx, cy);

			// Output the landmarks, pose, gaze, parameters and AUs
			outputAllFeatures(output_2D_landmarks, output_3D_landmarks, output_model_params, output_pose, output_AUs, output_gaze,
				clnf_model, frame_count, time_stamp, detection_success, gazeDirection0, gazeDirection1,
				pose_estimate, fx, fy, cx, cy, face_analyser);
		
			// detect key presses
			char character_press = cv::waitKey(1);
			
			// restart the tracker
			if(character_press == 'r')
			{
				clnf_model.Reset();
			}
			// quit the application
			else if(character_press=='q')
			{
				return(0);
			}

			// Update the frame count
			frame_count++;

		}
		
		frame_count = 0;

		// Reset the model, for the next video
		clnf_model.Reset();
		
		// break out of the loop if done with all the files (or using a webcam)
		if(f_n == files.size() -1 || files.empty())
		{
			done = true;
		}
	}

	return 0;
}

