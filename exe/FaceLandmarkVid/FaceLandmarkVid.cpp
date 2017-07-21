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

//Send 3D Landmarks via OSC Messeges
void send_landmarks_via_osc(char* oscAddress, cv::Mat_<double>& theLandmarks) {

	p.Clear();

	p << osc::BeginBundleImmediate
		<< osc::BeginMessage(oscAddress);

	for (int i = 0; i < theLandmarks.cols; i++) {
		for (int j = 0; j < theLandmarks.rows; j++) {
			p << (float)theLandmarks[j][i];
		}
	}

	p << osc::EndMessage
		<< osc::EndBundle;

	transmitSocket.Send(p.Data(), p.Size());

}


//Send Gaze Vectors via OSC Messeges
cv::Point3f GetPupilCenter(cv::Mat_<double> eyeLdmks3d) {

	eyeLdmks3d = eyeLdmks3d.t();

	cv::Mat_<double> irisLdmks3d = eyeLdmks3d.rowRange(0, 8);

	cv::Point3f pt(mean(irisLdmks3d.col(0))[0], mean(irisLdmks3d.col(1))[0], mean(irisLdmks3d.col(2))[0]);
	return pt;
}

void send_gaze_via_osc(char* oscAddress, cv::Mat eyeLdmks3d, cv::Point3f gazeVecAxis)
{
	//cv::Mat eyeLdmks3d_left = clnf_model.hierarchical_models[part_left].GetShape(fx, fy, cx, cy);
	cv::Point3f pupil_pos = GetPupilCenter(eyeLdmks3d);

	vector<cv::Point3d> points;
	points.push_back(cv::Point3d(pupil_pos));
	points.push_back(cv::Point3d(pupil_pos + gazeVecAxis*50.0));

	p.Clear();

	p << osc::BeginBundleImmediate
		<< osc::BeginMessage(oscAddress);

	p << (float)points[0].x << (float)points[0].y << (float)points[0].z << (float)points[1].x << (float)points[1].y << (float)points[1].z;

	p << osc::EndMessage
		<< osc::EndBundle;

	transmitSocket.Send(p.Data(), p.Size());
}


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

		
		//Send data via OSC ( 3 gaze coords + 68 3D landmarks ) 
		//#####################################################

		/*
			TO DO:
				- Add gestures: mouth width + height, eyebrows height, eye size, jaw pos
				- Add Action Units
		*/

		//store 3D face landmarks
		//fx,fy,cx,cy = Camera focal length and optical centre
		cv::Mat_<double> faceLandmarks3d = face_model.GetShape(fx, fy, cx, cy);

		//Store Eyes Landmarsks
		//detect which model holds which eye
		int part_left = -1;
		int part_right = -1;
		for (size_t i = 0; i < face_model.hierarchical_models.size(); ++i)
		{
			if (face_model.hierarchical_model_names[i].compare("left_eye_28") == 0)
			{
				part_left = i;
			}
			if (face_model.hierarchical_model_names[i].compare("right_eye_28") == 0)
			{
				part_right = i;
			}
		}
		//store the eyes landmarks
		cv::Mat_<double> rEyeLandmarks3d = face_model.hierarchical_models[part_left].GetShape(fx, fy, cx, cy);
		cv::Mat_<double> lEyeLandmarks3d = face_model.hierarchical_models[part_right].GetShape(fx, fy, cx, cy);

		//Send landmarks to OSC
		send_landmarks_via_osc("/openFace/faceLandmarks", faceLandmarks3d);
		send_landmarks_via_osc("/openFace/rightEye", rEyeLandmarks3d);
		send_landmarks_via_osc("/openFace/leftEye", lEyeLandmarks3d);
		
		//Send gaze vectors
		send_gaze_via_osc("/openFace/gazeVectorR", rEyeLandmarks3d, gazeDirection0);
		send_gaze_via_osc("/openFace/gazeVectorL", lEyeLandmarks3d, gazeDirection1);
		
		//Send Head Pose Vector
		//Position + Angle in radians (x, y, z, pitch_x, yaw_y, roll_z)
		p.Clear();

		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/openFace/headPose");

		for (int i = 0; i < pose_estimate_to_draw.rows; i++) {
			p << (float)pose_estimate_to_draw[i];
		}

		p << osc::EndMessage
			<< osc::EndBundle;

		transmitSocket.Send(p.Data(), p.Size());

		// END OF OSC Data Send

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
	int f_n = -1;
	
	det_parameters.track_gaze = true;

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
			
			//######### Add OSC calls here. #########


			// output the tracked video
			if (!output_video_files.empty())
			{
				writerFace << captured_image;
			}


			video_capture >> captured_image;
		
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

