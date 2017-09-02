# OSC_Transmitter usses LandmarkDetector and FaceAnalyser, include their headers
include_directories(../LandmarkDetector/include)
include_directories(../FaceAnalyser/include)
# include local headers
include_directories (.)

# add source files depending on platform
file (GLOB HEADERS *.h)
if(WIN32)
	file (GLOB SOURCE *.cpp ip/*.cpp osc/*.cpp ip/win32/*.cpp)
else()
	file (GLOB SOURCE *.cpp ip/*.cpp osc/*.cpp ip/posix/*.cpp)
endif()

add_library( OSC_Transmitter ${SOURCE} ${HEADERS} )

install (TARGETS OSC_Transmitter DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include/OpenFace)
