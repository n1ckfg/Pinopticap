#include "Eye.h"

Eye::Eye(string _hostName, string _sessionId, int _index) {
    hostName = _hostName;
    sessionId = _sessionId;
	index = _index;

	float r = ofRandom(127);
	float g = ofRandom(127);
	float b = ofRandom(127);
	idColor = ofColor(r + 127, g + 127, b + 127);
	bgColor = ofColor(r * 0.8, g * 0.8, b * 0.8);
	
	numSamples = 30;
	numBlobSequences = 200;
}

void Eye::update() {

}

void Eye::draw() {

}

void Eye::wsClientConnect(string hostName, int wsPort) {
	string wsName = hostName + ".local";
	Poco::Net::HTTPClientSession websocket(wsName, wsPort);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/?encoding=text", Poco::Net::HTTPMessage::HTTP_1_1);
	request.set("origin", "http://" + wsName);
	Poco::Net::HTTPResponse response;

	try {
		cout << "\nWebsocket client connecting to " + wsName << " on port " << wsPort << "..." << endl;
		wsClient = new Poco::Net::WebSocket(websocket, request, response);
		
		/*
		char const* testStr = "Hello echo websocket!";
		char receiveBuff[256];

		int len = wsClient->sendFrame(testStr, strlen(testStr), Poco::Net::WebSocket::FRAME_TEXT);
		std::cout << "Sent bytes " << len << std::endl;
		int flags = 0;

		int rlen = wsClient->receiveFrame(receiveBuff, 256, flags);
		std::cout << "Received bytes " << rlen << std::endl;
		std::cout << receiveBuff << std::endl;
		*/

		cout << "\nWebsocket client successfully connected to " + wsName << " on port " << wsPort << endl;
	} catch (std::exception& e) {
		cout << "Exception: " << e.what() << endl;
	}
}

void Eye::addBlob(int _index, float _x, float _y, int _timestamp, int _diff_timestamp) {   
    EyeBlob blob = EyeBlob();
    blob.index = _index;
    blob.x = _x;
    blob.y = _y;
    blob.timestamp = abs(_diff_timestamp - _timestamp);
    
	//cout << "Blob received, diff " << blob.timestamp << endl;

	int whichOne = checkUniqueBlob(blob.index);

	if (whichOne == -1) {
		cout << "Eye " << index << ", new blob " << whichOne << endl;
		EyeBlobSequence blobSequence = EyeBlobSequence();
		whichOne = blob.index;
		blobSequence.index = whichOne;
		blobSequences.push_back(blobSequence);
	}

    blobSequences[whichOne].blobs.push_back(blob);
	if (blobSequences[whichOne].blobs.size() > numSamples - 1) blobSequences[whichOne].blobs.erase(blobSequences[whichOne].blobs.begin());
	if (blobSequences.size() > numBlobSequences - 1) blobSequences.erase(blobSequences.begin());
}

int Eye::checkUniqueBlob(int _index) {
	int idx = -1;
	for (int i = 0; i < blobSequences.size(); i++) {
		if (_index == blobSequences[i].index) {
			idx = i;
			break;
		}
	}
	return idx;
}

void Eye::drawBlob() {
	//
}

void Eye::addVideo(ofImage _image, int _timestamp, int _diff_timestamp) {   
    EyeVideo video = EyeVideo();
    video.image = _image;
    video.timestamp = abs(_diff_timestamp - _timestamp);
    
	//cout << "Video received, diff " << video.timestamp << endl;

    videos.push_back(video);
	if (videos.size() > numSamples) videos.erase(videos.begin());
}

void Eye::drawVideo(int x, int y, int w, int h) {
	ofSetColor(255);
	if (videos.size() > 0) videos[videos.size() - 1].image.draw(x, y, w, h);
}

void Eye::addContour() {
	// TODO
}

void Eye::drawContour() {
	// TODO
}