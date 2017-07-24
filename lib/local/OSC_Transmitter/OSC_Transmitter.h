// OSC_Transmitter.h

//OSC Library Includes
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

// Libraries for landmark detection (includes CLNF and CLM modules)
#include "LandmarkCoreIncludes.h"
#include "GazeEstimation.h"

// OpenCV includes
#include <opencv2/videoio/videoio.hpp>  // Video write
#include <opencv2/videoio/videoio_c.h>  // Video write
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace OSC_Funcs
{
	class OSC_Transmitter
	{
	public:

		// A default constructor
		OSC_Transmitter();
		
		// Sends OSC Messege
		static void SendMessege(const LandmarkDetector::CLNF& face_model, cv::Point3f gazeDirection0, cv::Point3f gazeDirection1, double fx, double fy, double cx, double cy);

	};
}