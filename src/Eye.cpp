#include "Eye.h"

Eye::Eye(string _hostName, string _uniqueId, int _index) {
    hostName = _hostName;
    uniqueId = _uniqueId;
	index = _index;

	float r = ofRandom(127);
	float g = ofRandom(127);
	float b = ofRandom(127);
	idColor = ofColor(r + 127, g + 127, b + 127);
	bgColor = ofColor(r * 0.8, g * 0.8, b * 0.8);
	
	numSamples = 30;
	numBlobSequences = 200;
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
	if (videos.size() > numSamples - 1) videos.erase(videos.begin());
}

void Eye::drawVideo() {
	//
}