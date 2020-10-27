#include "Eye.h"

Eye::Eye(string _hostName, string _uniqueId) {
    hostName = _hostName;
    uniqueId = _uniqueId;
}

void Eye::addBlob(int _index, float _x, float _y, int _timestamp, int _diff_timestamp) {
    if (blobs.size() > numSamples-1) blobs.erase(blobs.begin());
    
    EyeBlob blob = EyeBlob();
    blob.index = _index;
    blob.x = _x;
    blob.y = _y;
    blob.timestamp = abs(_diff_timestamp - _timestamp);
    
    blobs.push_back(blob);
}

void Eye::addVideo(ofBuffer _buffer, int _timestamp, int _diff_timestamp) {
    if (videos.size() > numSamples-1) videos.erase(videos.begin());
    
    EyeVideo video = EyeVideo();
    video.image.allocate(640, 480, OF_IMAGE_GRAYSCALE);
    video.image.load(_buffer);
    video.timestamp = abs(_diff_timestamp - _timestamp);
    
    videos.push_back(video);
}
