#TBB library
include_directories(${TBB_ROOT_DIR}/include)

# Local libraries
include_directories(${LandmarkDetector_SOURCE_DIR}/include)
	
include_directories(../../lib/local/LandmarkDetector/include)
include_directories(../../lib/local/FaceAnalyser/include)
include_directories(../../lib/local/OSC_Transmitter)

			
add_executable(FaceLandmarkVidMulti FaceLandmarkVidMulti.cpp)

target_link_libraries(FaceLandmarkVidMulti LandmarkDetector)
target_link_libraries(FaceLandmarkVidMulti OSC_Transmitter)
target_link_libraries(FaceLandmarkVidMulti dlib)
target_link_libraries(FaceLandmarkVidMulti ${OpenCV_LIBS} ${Boost_LIBRARIES} ${TBB_LIBRARIES})

install (TARGETS FaceLandmarkVidMulti DESTINATION bin)
