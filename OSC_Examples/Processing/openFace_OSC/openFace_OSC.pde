import oscP5.*;
import netP5.*;
  
OscP5 oscP5;
NetAddress myRemoteLocation;

//An array to store main face landmarks
int faceArrayCols = 2;
int faceArrayRows = 68;
float[][] faceLandmarks = new float[faceArrayCols][faceArrayRows];

void setup() {
  size(800,800);
  frameRate(30);
  /* start oscP5, listening for incoming messages at port 6448 */
  oscP5 = new OscP5(this,6448);
  
  /* myRemoteLocation is a NetAddress. a NetAddress takes 2 parameters,
   * an ip address and a port number. myRemoteLocation is used as parameter in
   * oscP5.send() when sending osc packets to another computer, device, 
   * application. usage see below. for testing purposes the listening port
   * and the port of the remote location address are the same, hence you will
   * send messages back to this sketch.
   */
  //myRemoteLocation = new NetAddress("127.0.0.1",6448);
  
  background(0);
}


void draw() {

  background(0);
  fill(100);
  for (int i = 0; i < faceArrayRows; i++) {
   ellipse(faceLandmarks[0][i], faceLandmarks[1][i], 5, 5);
  }

}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {

  //  background(0);
  fill(150);
  
  /* print the address pattern and the typetag of the received OscMessage */
  //print("### received an osc message.");
  //println(" addr: "+theOscMessage.addrPattern() + " ");

  //Store incoming Face landmarks
  for (int i = 0; i< faceArrayRows; i++) {
    faceLandmarks[0][i] = theOscMessage.get(i).floatValue();
    faceLandmarks[1][i] = theOscMessage.get(i + faceArrayRows).floatValue();
    //println("Landmark " + i + ": " + faceLandmarks[0][i] + " / "+ faceLandmarks[1][i]);  
  }
  
  //println(" # ");

}