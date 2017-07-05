import oscP5.*;
import netP5.*;
  
OscP5 oscP5;
NetAddress myRemoteLocation;

int cols = 2;
int rows = 68;
float[][] faceLandmarks = new float[cols][rows];

void setup() {
  size(800,800);
  frameRate(30);
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,6448);
  
  /* myRemoteLocation is a NetAddress. a NetAddress takes 2 parameters,
   * an ip address and a port number. myRemoteLocation is used as parameter in
   * oscP5.send() when sending osc packets to another computer, device, 
   * application. usage see below. for testing purposes the listening port
   * and the port of the remote location address are the same, hence you will
   * send messages back to this sketch.
   */
  myRemoteLocation = new NetAddress("127.0.0.1",6448);
  
  background(0);
}


void draw() {

  background(0);
  fill(100);
  for (int i = 0; i < 68; i++) {
   ellipse(faceLandmarks[0][i], faceLandmarks[1][i], 5, 5);
  }

}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {

  //  background(0);
  fill(150);
  
  /* print the address pattern and the typetag of the received OscMessage */
  print("### received an osc message.");
  println(" addr: "+theOscMessage.addrPattern() + " ");

  for (int i = 0; i< 68; i++) {
    //print(theOscMessage.get(i).floatValue() + " " );
    faceLandmarks[0][i] = theOscMessage.get(i).floatValue();
    faceLandmarks[1][i] = theOscMessage.get(i + 64).floatValue();
    println("Landmark " + i + ": " + faceLandmarks[0][i] + " / "+ faceLandmarks[1][i]);  
  }
  
  //println(" # ");

}