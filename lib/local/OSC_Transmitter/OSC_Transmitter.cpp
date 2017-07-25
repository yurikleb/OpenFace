// OSC_Transmitter.cpp

//Send data via OSC ( 3 gaze coords + 68 3D landmarks ) 
//#####################################################

/*
TO DOs:

- Action Units Send
- Separate Action Units from pose
- Add gestures: mouth width + height, eyebrows height, eye size, jaw pos
- Add settings flags or UI to be able to chnage remote IP/PORT

*/

#include "OSC_Transmitter.h"

//OSC Settings
#define ADDRESS "127.0.0.1"
#define PORT 6448
#define OUTPUT_BUFFER_SIZE 2500

UdpTransmitSocket oscTransmitSocket(IpEndpointName(ADDRESS, PORT));

char oscBuffer[OUTPUT_BUFFER_SIZE];
osc::OutboundPacketStream packet(oscBuffer, OUTPUT_BUFFER_SIZE);

//Define Console Log Channels
#define INFO_STREAM( stream ) \
std::cout << stream << std::endl

#define WARN_STREAM( stream ) \
std::cout << "Warning: " << stream << std::endl

#define ERROR_STREAM( stream ) \
std::cout << "Error: " << stream << std::endl

using namespace std;

namespace OSC_Funcs
{

	OSC_Transmitter::OSC_Transmitter() {
		//OSC INIT
		//(void)argc; // suppress unused parameter warnings
		//(void)argv; // suppress unused parameter warnings
		//INFO_STREAM("OSC Transmitter Online!");
	}

	//Send 3D Landmarks via OSC Messeges
	void send_landmarks_via_osc(char* oscAddress, cv::Mat_<double>& theLandmarks) {

		packet.Clear();

		packet << osc::BeginBundleImmediate
			<< osc::BeginMessage(oscAddress);

		for (int i = 0; i < theLandmarks.cols; i++) {
			for (int j = 0; j < theLandmarks.rows; j++) {
				packet << (float)theLandmarks[j][i];
			}
		}

		packet << osc::EndMessage
			<< osc::EndBundle;

		oscTransmitSocket.Send(packet.Data(), packet.Size());

	}


	//Get Pupil Center
	cv::Point3f GetPupilCenter(cv::Mat_<double> eyeLdmks3d) {

		eyeLdmks3d = eyeLdmks3d.t();

		cv::Mat_<double> irisLdmks3d = eyeLdmks3d.rowRange(0, 8);

		cv::Point3f pt(mean(irisLdmks3d.col(0))[0], mean(irisLdmks3d.col(1))[0], mean(irisLdmks3d.col(2))[0]);
		return pt;
	}

	//Send Gaze Vectors
	void send_gaze_via_osc(char* oscAddress, cv::Mat eyeLdmks3d, cv::Point3f gazeVecAxis)
	{
		//cv::Mat eyeLdmks3d_left = clnf_model.hierarchical_models[part_left].GetShape(fx, fy, cx, cy);
		cv::Point3f pupil_pos = GetPupilCenter(eyeLdmks3d);

		vector<cv::Point3d> points;
		points.push_back(cv::Point3d(pupil_pos));
		points.push_back(cv::Point3d(pupil_pos + gazeVecAxis*50.0));

		packet.Clear();

		packet << osc::BeginBundleImmediate
			<< osc::BeginMessage(oscAddress);

		packet << (float)points[0].x << (float)points[0].y << (float)points[0].z << (float)points[1].x << (float)points[1].y << (float)points[1].z;

		packet << osc::EndMessage
			<< osc::EndBundle;

		oscTransmitSocket.Send(packet.Data(), packet.Size());
	}

	//Send Face Data Over OSC used in FaceLandmarkVid.cpp and in FaceLandmarkVidMulti.cpp
	void OSC_Transmitter::SendFaceData(const LandmarkDetector::CLNF& face_model, cv::Point3f gazeDirection0, cv::Point3f gazeDirection1, double fx, double fy, double cx, double cy, int modelId) {

		string oscAddressPrefix = "/openFace/";
		
		if (modelId > -1) {
			oscAddressPrefix += "faceId_" + to_string(modelId) + "/";
			//cout << oscAddressPrefix << "\n";
		}
	
		//store 3D face landmarks
		//fx,fy,cx,cy = Camera focal length and optical centre
		cv::Mat_<double> userFaceLandmarks3d = face_model.GetShape(fx, fy, cx, cy);

		//EYE LANDMARKS
		//Detect which model holds which eye
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
		send_landmarks_via_osc("/openFace/faceLandmarks", userFaceLandmarks3d);
		send_landmarks_via_osc("/openFace/rightEye", rEyeLandmarks3d);
		send_landmarks_via_osc("/openFace/leftEye", lEyeLandmarks3d);

		//Send gaze vectors
		if (gazeDirection0 != cv::Point3f(0, 0, 0) && gazeDirection1 != cv::Point3f(0, 0, 0)) {
			send_gaze_via_osc("/openFace/gazeVectorR", rEyeLandmarks3d, gazeDirection0);
			send_gaze_via_osc("/openFace/gazeVectorL", lEyeLandmarks3d, gazeDirection1);
		}

		//Send Head Pose Vector: Position + Angle in radians (x, y, z, pitch_x, yaw_y, roll_z)
		cv::Vec6d pose_estimate_to_draw = LandmarkDetector::GetCorrectedPoseWorld(face_model, fx, fy, cx, cy);

		packet.Clear();

		packet << osc::BeginBundleImmediate
			<< osc::BeginMessage("/openFace/headPose");

		for (int i = 0; i < pose_estimate_to_draw.rows; i++) {
			packet << (float)pose_estimate_to_draw[i];
		}

		packet << osc::EndMessage
			<< osc::EndBundle;

		oscTransmitSocket.Send(packet.Data(), packet.Size());

	}


}