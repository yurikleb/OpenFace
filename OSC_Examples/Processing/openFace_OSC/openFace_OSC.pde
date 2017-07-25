//This Is a Sample sketch demonstraitng reading and visualizing  OSC data coming form OpenFace.
// Make sure you have oscP5 Library installed.

import oscP5.*;
import netP5.*;
  
OscP5 oscP5;
NetAddress myRemoteLocation;

//Array to store  face / eyes landmarks
int faceArrayCols = 68;
int eyeArrayCols = 28;
int landmarkDimensions = 3;

float[][] faceLandmarks = new float[faceArrayCols][landmarkDimensions];
float[][] rEyeLandmarks = new float[eyeArrayCols][landmarkDimensions];
float[][] lEyeLandmarks = new float[eyeArrayCols][landmarkDimensions];

PVector[] rEyeGazeVec = new PVector[2];
PVector[] lEyeGazeVec = new PVector[2];

float[] headPose = new float[6];

float midX,midY,midZ;

void setup() {
  
  size(800,800,P3D);
  frameRate(30);
  midX = width/2;
  midY = height/2;
  midZ = 700;
  
  rEyeGazeVec[0] = new PVector(0,0,0);
  rEyeGazeVec[1] = new PVector(0,0,0);
  lEyeGazeVec[0] = new PVector(0,0,0);
  lEyeGazeVec[1] = new PVector(0,0,0);
  
  // Start oscP5
  // Listening for incoming messages at port 6448
  oscP5 = new OscP5(this,6448);
  
  background(0);
  textSize(7);

}


void draw() {

  background(0);
  fill(200);
  noStroke();
  
  translate(midX, midY, midZ);  
  
  //Draw Face landmarks
  for (int i = 0; i < faceArrayCols; i++) {
    pushMatrix();
    translate(faceLandmarks[i][0], faceLandmarks[i][1]  , -faceLandmarks[i][2]); 
    sphere(0.5);
    popMatrix();
  }
  
  //Draw Eyes
  drawEye(rEyeLandmarks, color(255,0,0));
  drawEye(lEyeLandmarks, color(0,255,0));
  
  //Draw Gaze Vectors
  noFill();
  stroke(255);
  pushMatrix();
  line(lEyeGazeVec[0].x, lEyeGazeVec[0].y, -lEyeGazeVec[0].z, lEyeGazeVec[1].x, lEyeGazeVec[1].y, -lEyeGazeVec[1].z);
  line(rEyeGazeVec[0].x, rEyeGazeVec[0].y, -rEyeGazeVec[0].z, rEyeGazeVec[1].x, rEyeGazeVec[1].y, -rEyeGazeVec[1].z);
  popMatrix();
 
  //Draw head pose
  pushMatrix();
  translate(headPose[0], headPose[1], -headPose[2]);
  rotateX(-headPose[3]);
  rotateY(-headPose[4]);
  rotateZ(headPose[5]);  
  noFill();
  stroke(100,0,100);
  box(150);
  popMatrix();
  
}

//Draw Eye landmarks: 8-19 Eyelids, 0-7 Iris, 20-27 Pupil
void drawEye(float eyeLandmarks[][], color col){

  noFill();  
  stroke(col);
  
  // Draw iris (Landmarks 0-7 in the eye landmarks matrix)
  beginShape();
  for (int i = 0; i < 8; i++) {
    pushMatrix();
    vertex(eyeLandmarks[i][0], eyeLandmarks[i][1], -eyeLandmarks[i][2]);
    popMatrix();
   }
   vertex(eyeLandmarks[0][0], eyeLandmarks[0][1], -eyeLandmarks[0][2]);   
   endShape();
   
  // Draw eyelids (Landmarks 8-19 in the eye landmarks matrix)
  beginShape();
  for (int i = 8; i < 20; i++) {
    pushMatrix();
    vertex(eyeLandmarks[i][0], eyeLandmarks[i][1], -eyeLandmarks[i][2]);
    popMatrix();
   }
   vertex(eyeLandmarks[8][0], eyeLandmarks[8][1], -eyeLandmarks[8][2]);
   endShape();
  
  //Draw pupil (Landmarks 20-27 in the eye landmarks matrix)
  beginShape();
  for (int i = 20; i < 28; i++) {
    pushMatrix();
    vertex(eyeLandmarks[i][0], eyeLandmarks[i][1], -eyeLandmarks[i][2]);
    popMatrix();
   }
   vertex(eyeLandmarks[20][0], eyeLandmarks[20][1], -eyeLandmarks[20][2]);
   endShape();   
}


//Incoming osc message are forwarded to the oscEvent method.
//Here we parse the OSC messeges

void oscEvent(OscMessage theOscMessage) {
  
  //print the address pattern and the typetag of the received OscMessage */
  //Uncomment this to see all incoming OSC messages/data.
  //println("### received an osc message");
  //print(" addr: "+theOscMessage.addrPattern() + " ");
  //theOscMessage.print();
  //println();
  //println("### end of osc message ###");

  //Store incoming Face landmarks
  if(theOscMessage.checkAddrPattern("/openFace/faceLandmarks")==true){
    for (int i = 0; i< faceArrayCols; i++) {
      for (int j = 0; j < landmarkDimensions; j++){
        faceLandmarks[i][j] = theOscMessage.get(i * landmarkDimensions + j).floatValue();
      }
    }
   }
   
   //Store incoming left eye landmarks
   if(theOscMessage.checkAddrPattern("/openFace/leftEye")==true){
     for (int i = 0; i < eyeArrayCols; i++) {
       for (int j = 0; j < landmarkDimensions; j++){
         lEyeLandmarks[i][j] = theOscMessage.get(i * landmarkDimensions + j).floatValue();
       }
     }
   }
   
   //Store incoming right eye landmarks
   if(theOscMessage.checkAddrPattern("/openFace/rightEye")==true){
     for (int i = 0; i < eyeArrayCols; i++) {
       for (int j = 0; j < landmarkDimensions; j++){
         rEyeLandmarks[i][j] = theOscMessage.get(i * landmarkDimensions + j).floatValue();
       }
     }
   }
   
   //Store incoming right eye gaze vector
   if(theOscMessage.checkAddrPattern("/openFace/gazeVectorR")==true){
     rEyeGazeVec[0].x = theOscMessage.get(0).floatValue();
     rEyeGazeVec[0].y = theOscMessage.get(1).floatValue();
     rEyeGazeVec[0].z = theOscMessage.get(2).floatValue();
     rEyeGazeVec[1].x = theOscMessage.get(3).floatValue();
     rEyeGazeVec[1].y = theOscMessage.get(4).floatValue();
     rEyeGazeVec[1].z = theOscMessage.get(5).floatValue();
   }
   
   //Store incoming right eye gaze vector
   if(theOscMessage.checkAddrPattern("/openFace/gazeVectorL")==true){
     lEyeGazeVec[0].x = theOscMessage.get(0).floatValue();
     lEyeGazeVec[0].y = theOscMessage.get(1).floatValue();
     lEyeGazeVec[0].z = theOscMessage.get(2).floatValue();
     lEyeGazeVec[1].x = theOscMessage.get(3).floatValue();
     lEyeGazeVec[1].y = theOscMessage.get(4).floatValue();
     lEyeGazeVec[1].z = theOscMessage.get(5).floatValue();
   } 
   
   //Store Head Pose
   //Headpose consissts of 6 values: x,y,z,rotation_x,rotation_y,rotation_z
   if(theOscMessage.checkAddrPattern("/openFace/headPose")==true){   
     for (int i = 0; i < 6; i++) {
       headPose[i] = theOscMessage.get(i).floatValue();
     }
   }

}