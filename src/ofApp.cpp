#include "ofApp.h"

using namespace cv;
using namespace ofxCv;

//--------------------------------------------------------------
void ofApp::setup() {
    settings.loadFile("settings.xml");
    
    ofSetVerticalSync(false);
    ofHideCursor();

    appFramerate = settings.getValue("settings:app_framerate", 60);
    camFramerate = settings.getValue("settings:cam_framerate", 30);
    ofSetFrameRate(appFramerate);

    syncVideoQuality = settings.getValue("settings:osc_video_quality", 3); 
    videoColor = (bool) settings.getValue("settings:video_color", 0); 
    
    width = settings.getValue("settings:width", 640);
    height = settings.getValue("settings:height", 480);
    ofSetWindowShape(width, height);

    debug = (bool) settings.getValue("settings:debug", 1);

    oscHost = settings.getValue("settings:osc_host", "127.0.0.1");
    oscPort = settings.getValue("settings:osc_port", 7114);
    oscReceivePort = settings.getValue("settings:osc_receive_port", 7110);
 
    debug = (bool) settings.getValue("settings:debug", 1);

    // camera
    if (videoColor) {
        gray.allocate(width, height, OF_IMAGE_COLOR);
    } else {
        gray.allocate(width, height, OF_IMAGE_GRAYSCALE);        
    }
    
    // ~ ~ ~   get a persistent name for this computer   ~ ~ ~
    // a randomly generated id
    uniqueId = "RPi";
    file.open(ofToDataPath("unique_id.txt"), ofFile::ReadWrite, false);
    ofBuffer buff;
    if (file) { // use existing file if it's there
        buff = file.readToBuffer();
        uniqueId = buff.getText();
    } else { // otherwise make a new one
        uniqueId += "_" + ofGetTimestampString("%y%m%d%H%M%S%i");
        ofStringReplace(uniqueId, "\n", "");
        ofStringReplace(uniqueId, "\r", "");
        buff.set(uniqueId.c_str(), uniqueId.size());
        ofBufferToFile("unique_id.txt", buff);
    }
   
    // the actual RPi hostname
    ofSystem("cp /etc/hostname " + ofToDataPath("DocumentRoot/js/"));
    hostName = ofSystem("cat /etc/hostname");
    hostName.pop_back(); // last char is \n
    
    fbo.allocate(width, height, GL_RGBA);
    pixels.allocate(width, height, OF_IMAGE_COLOR);
                
    sender.setup(oscHost, oscPort);
    receiver.setup(oscReceivePort);
    cout << "Using OSC." << endl;
}

//--------------------------------------------------------------
void ofApp::update() {
    timestamp = (int) ofGetSystemTimeMillis();
    
        // check for waiting messages
        while (receiver.hasWaitingMessages()) {
            // get the next message
            ofxOscMessage m;
            receiver.getNextMessage(m);

            // check for mouse moved message
            if (m.getAddress() == "/mouse/position") {
                // both the arguments are floats
                //mouseXf = m.getArgAsFloat(0);
                //mouseYf = m.getArgAsFloat(1);
            }  else if (m.getAddress() == "/mouse/button") {
                // first argument is int32, second is a string
                //mouseButtonInt = m.getArgAsInt32(0);
                //mouseButtonState = m.getArgAsString(1);
            } else if (m.getAddress() == "/image") {
                ofBuffer buffer = m.getArgAsBlob(0);
                //receivedImage.load(buffer);
            } else {
                // unrecognized message: display on the bottom of the screen
                string msgString;
                msgString = m.getAddress();
                msgString += ":";
                for (size_t i = 0; i < m.getNumArgs(); i++) {
                    // get the argument type
                    msgString += " ";
                    msgString += m.getArgTypeName(i);
                    msgString += ":";

                    // display the argument - make sure we get the right type
                    if (m.getArgType(i) == OFXOSC_TYPE_INT32) {
                        msgString += ofToString(m.getArgAsInt32(i));
                    } else if(m.getArgType(i) == OFXOSC_TYPE_FLOAT) {
                        msgString += ofToString(m.getArgAsFloat(i));
                    } else if(m.getArgType(i) == OFXOSC_TYPE_STRING) {
                        msgString += m.getArgAsString(i);
                    } else {
                        msgString += "unhandled argument type " + m.getArgTypeName(i);
                    }
                }

                // add to the list of strings to display
                //msgStrings[currentMsgString] = msgString;
                //timers[currentMsgString] = ofGetElapsedTimef() + 5.0f;
                //currentMsgString = (currentMsgString + 1) % NUM_MSG_STRINGS;

                // clear the next line
                //msgStrings[currentMsgString] = "";
            }
        }
}

//--------------------------------------------------------------
void ofApp::draw() {
    ofBackground(0);

    if(!frame.empty()) {
        if (debug) {
            if (!blobs && !contours) {
                drawMat(frame, 0, 0);
            } else if (blobs || contours) {
                drawMat(frameProcessed, 0, 0);
            }
        }

        if (syncVideo) {
            if (sendOsc) sendOscVideo();
        }

        if (blobs) {
            if (debug) {
            	ofSetLineWidth(2);
            	ofNoFill();
            }
            
            //autothreshold(frameProcessed);        
            threshold(frame, frameProcessed, thresholdValue, 255, 0);
            contourFinder.setThreshold(contourThreshold);    
            contourFinder.findContours(frameProcessed);

            int n = contourFinder.size();
            for (int i = 0; i < n; i++) {
                float circleRadius;
                glm::vec2 circleCenter = toOf(contourFinder.getMinEnclosingCircle(i, circleRadius));
                if (debug) {
                	ofSetColor(cyanPrint);
                	ofDrawCircle(circleCenter, circleRadius);
                	ofDrawCircle(circleCenter, 1);
                }

                if (sendOsc) sendOscBlobs(i, circleCenter.x, circleCenter.y);
                if (sendWs) sendWsBlobs(i, circleCenter.x, circleCenter.y);
            }
        }
    }

    if (debug) {
        stringstream info;
        info << width << "x" << height << " @ " << ofGetFrameRate() << "fps" << " / " << timestamp << "\n";
        ofDrawBitmapStringHighlight(info.str(), 10, 10, ofColor::black, ofColor::yellow);
    }
}

void ofApp::sendOscBlobs(int index, float x, float y) {
    ofxOscMessage m;
    m.setAddress("/blob");

    m.addStringArg(hostName);   
    m.addStringArg(uniqueId);
    m.addIntArg(index);  
    m.addFloatArg(x);
    m.addFloatArg(y);
    m.addFloatArg(z);
    m.addIntArg(timestamp);

    sender.sendMessage(m);
}



