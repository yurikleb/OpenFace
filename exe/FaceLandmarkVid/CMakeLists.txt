#TBB library
include_directories(${TBB_ROOT_DIR}/include)

add_executable(FaceLandmarkVid FaceLandmarkVid.cpp)

# Local libraries
include_directories(${LandmarkDetector_SOURCE_DIR}/include)

include_directories(../../lib/local/LandmarkDetector/include)
include_directories(../../lib/local/FaceAnalyser/include)

target_link_libraries(FaceLandmarkVid LandmarkDetector)
target_link_libraries(FaceLandmarkVid FaceAnalyser)

target_link_libraries(FaceLandmarkVid dlib)

target_link_libraries(FaceLandmarkVid ${OpenCV_LIBS} ${Boost_LIBRARIES} ${TBB_LIBRARIES})

#TODO swap absolute path for a relative CMAKE variable
target_link_libraries(FaceLandmarkVid /Users/George/Downloads/Grouped/cv/OpenFace/exe/FaceLandmarkVid/liboscpack.a)


install (TARGETS FaceLandmarkVid DESTINATION bin)

