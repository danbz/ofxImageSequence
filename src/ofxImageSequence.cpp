/**
 *  ofxImageSequence.cpp
 *
 * Created by James George, http://www.jamesgeorge.org
 * in collaboration with FlightPhase http://www.flightphase.com
 *		- Updated for 0.8.4 by James George on 12/10/2014 for Specular (http://specular.cc) (how time flies!) 
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------
 *
 *  ofxImageSequence is a class for easily loading a series of image files
 *  and accessing them like you would frames of a movie.
 *  
 *  This class loads only textures to the graphics card and does not store pixel data in memory. This helps with
 *  fast, random access drawing of seuqences
 *  
 *  Why would you use this instead of a movie file? A few reasons,
 *  If you want truly random frame access with no lag on large images, ofxImageSequence allows it
 *  If you need a movie with alpha channel the only readily available codec is Animation (PNG) which is slow at large resolutions, so this class can help with that
 *  If you want to easily access frames based on percents this class makes that easy
 *  
 */

#include "ofxImageSequence.h"

ofxImageSequence::ofxImageSequence()
{
	loaded = false;
	loading = false;
	useThread = false;
	cancelLoading = false;
	frameRate = 30.0f;
	lastFrameLoaded = -1;
	currentFrame = 0;
	maxFrames = 0;
}

ofxImageSequence::~ofxImageSequence()
{
	if(useThread && loading){
		cancelLoad();
	}
	
	waitForThread(true);

	unloadSequence();
}

bool ofxImageSequence::loadSequence(string prefix, string filetype,  int startDigit, int endDigit)
{
	return loadSequence(prefix, filetype, startDigit, endDigit, 0);
}

bool ofxImageSequence::loadSequence(string prefix, string filetype,  int startDigit, int endDigit, int numDigits)
{
	unloadSequence();

	char imagename[1024];
	stringstream format;
	int numFiles = endDigit - startDigit+1;
	if(numFiles <= 0 ){
		ofLogError("ofxImageSequence::loadSequence") << "No image files found.";
		return false;
	}

	if(numDigits != 0){
		format <<prefix<<"%0"<<numDigits<<"d."<<filetype;
	} else{
		format <<prefix<<"%d."<<filetype; 
	}
	
	for(int i = startDigit; i <= endDigit; i++){
		sprintf(imagename, format.str().c_str(), i);
		filenames.push_back(imagename);
		sequence.push_back(ofPixels());
	}
	
	loaded = true;
	
	lastFrameLoaded = -1;
	loadFrame(0);
	
	width  = sequence[0].getWidth();
	height = sequence[0].getHeight();
	return true;
}

bool ofxImageSequence::loadSequence(string _folder)
{
	unloadSequence();

	folderToLoad = _folder;

	if(useThread){
		startThread(true);
		return true;
	}

	if(preloadAllFilenames()){
		completeLoading();
		return true;
	}
	
	return false;

}

void ofxImageSequence::completeLoading()
{

	if(sequence.size() == 0){
		ofLogError("ofxImageSequence::completeLoading") << "load failed with empty image sequence";
		return;
	}

	loaded = true;	
	lastFrameLoaded = -1;
	loadFrame(0);
	
	width  = sequence[0].getWidth();
	height = sequence[0].getHeight();

}

bool ofxImageSequence::preloadAllFilenames()
{
    ofDirectory dir;
	if(extension != ""){
		dir.allowExt(extension);
	}
	
	if(!ofFile(folderToLoad).exists()){
		ofLogError("ofxImageSequence::loadSequence") << "Could not find folder " << folderToLoad;
		return false;
	}

	int numFiles;
	if(maxFrames > 0){
		numFiles = MIN(dir.listDir(folderToLoad), maxFrames);
	}
	else{	
		numFiles = dir.listDir(folderToLoad);
	}

    if(numFiles == 0) {
		ofLogError("ofxImageSequence::loadSequence") << "No image files found in " << folderToLoad;
		return false;
	}

    // read the directory for the images
	#ifdef TARGET_LINUX
	dir.sort();
	#endif

	for(int i = 0; i < numFiles; i++) {
        filenames.push_back(dir.getPath(i));
		sequence.push_back(ofPixels());
    }
	return true;
}

void ofxImageSequence::threadedFunction()
{

	loading = true;
	cancelLoading = false;

	ofAddListener(ofEvents().update, this, &ofxImageSequence::updateThreadedLoad);

	if(!preloadAllFilenames()){
		loading = false;
		return;
	}

	if(cancelLoading){
		loading = false;
		cancelLoading = false;
		return;
	}
	
	preloadAllFrames();
	
	loading = false;
}

void ofxImageSequence::updateThreadedLoad(ofEventArgs& args){
	if(loading){
		return;
	}
	
	ofRemoveListener(ofEvents().update, this, &ofxImageSequence::updateThreadedLoad);

	if(sequence.size() > 0){
		completeLoading();
	}
}

//set to limit the number of frames. negative means no limit
void ofxImageSequence::setMaxFrames(int newMaxFrames)
{
	maxFrames = MAX(newMaxFrames, 0);
	if(loaded){
		ofLogError("ofxImageSequence::setMaxFrames") << "Max frames must be called before load";
	}
}

void ofxImageSequence::setExtension(string ext)
{
	extension = ext;
}

void ofxImageSequence::enableThreadedLoad(bool enable){

	if(loaded){
		ofLogError("ofxImageSequence::enableThreadedLoad") << "Need to enable threaded loading before calling load";
	}
	useThread = enable;
}

void ofxImageSequence::cancelLoad()
{
	if(useThread && loading){
		cancelLoading = true;
	}
}

void ofxImageSequence::setMinMagFilter(int newMinFilter, int newMagFilter)
{
	minFilter = newMinFilter;
	magFilter = newMagFilter;
	texture.setTextureMinMagFilter(minFilter, magFilter);
}

void ofxImageSequence::preloadAllFrames()
{
	if(sequence.size() == 0){
		ofLogError("ofxImageSequence::loadFrame") << "Calling preloadAllFrames on unitialized image sequence.";
		return;
	}
	
	for(int i = 0; i < sequence.size(); i++){
		//threaded stuff
		if(useThread){
			ofSleepMillis(5);
			if(cancelLoading){
				return;
			}
		}

		if(!ofLoadImage(sequence[i], filenames[i])){
			ofLogError("ofxImageSequence::loadFrame") << "Image failed to load: " << filenames[i];		
		}
	}
}

void ofxImageSequence::loadFrame(int imageIndex)
{
	if(lastFrameLoaded == imageIndex){
		return;
	}

	if(imageIndex < 0 || imageIndex >= sequence.size()){
		ofLogError("ofxImageSequence::loadFrame") << "Calling a frame out of bounds: " << imageIndex;
		return;
	}

	if(!sequence[imageIndex].isAllocated()){
		ofLoadImage(sequence[imageIndex], filenames[imageIndex]);
	}

	if(!sequence[imageIndex].isAllocated()){
		ofLogError("ofxImageSequence::loadFrame") << "Pixels not allocated: " << filenames[imageIndex];
		return;
	}

	texture.loadData(sequence[imageIndex]);

	lastFrameLoaded = imageIndex;

}

float ofxImageSequence::getPercentAtFrameIndex(int index)
{
	return ofMap(index, 0, sequence.size()-1, 0, 1.0, true);
}

float ofxImageSequence::getWidth()
{
	return width;
}

float ofxImageSequence::getHeight()
{
	return height;
}

void ofxImageSequence::unloadSequence()
{
	
	waitForThread(true);

	sequence.clear();
	filenames.clear();
	
	loaded = false;
	width = 0;
	height = 0;

}

void ofxImageSequence::setFrameRate(float rate)
{
	frameRate = rate;
}

int ofxImageSequence::getFrameIndexAtPercent(float percent)
{
    if (percent < 0.0 || percent > 1.0) percent -= floor(percent);

	return MIN((int)(percent*sequence.size()), sequence.size()-1);
}

//deprecated
ofTexture& ofxImageSequence::getTextureReference()
{
	return getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrameAtPercent(float percent)
{
	setFrameAtPercent(percent);
	return &getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrameForTime(float time)
{
	setFrameForTime(time);
	return &getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrame(int index)
{
	setFrame(index);
	return &getTexture();
}

ofTexture& ofxImageSequence::getTextureForFrame(int index)
{
	setFrame(index);
	return getTexture();
}

ofTexture& ofxImageSequence::getTextureForTime(float time)
{
	setFrameForTime(time);
	return getTexture();
}

ofTexture& ofxImageSequence::getTextureForPercent(float percent){
	setFrameAtPercent(percent);
	return getTexture();
}

void ofxImageSequence::setFrame(int index)
{
	if(!loaded){
		ofLogError("ofxImageSequence::setFrame") << "Calling getFrame on unitialized image sequence.";
		return;
	}

	if(index < 0){
		ofLogError("ofxImageSequence::setFrame") << "Asking for negative index.";
		return;
	}
	
	index %= getTotalFrames();
	
	loadFrame(index);
	currentFrame = index;
}

void ofxImageSequence::setFrameForTime(float time)
{
	float totalTime = sequence.size() / frameRate;
	float percent = time / totalTime;
	return setFrameAtPercent(percent);	
}

void ofxImageSequence::setFrameAtPercent(float percent)
{
	setFrame(getFrameIndexAtPercent(percent));	
}

ofTexture& ofxImageSequence::getTexture()
{
	return texture;
}

const ofTexture& ofxImageSequence::getTexture() const
{
	return texture;
}

float ofxImageSequence::getLengthInSeconds()
{
	return getTotalFrames() / frameRate;
}

int ofxImageSequence::getTotalFrames()
{
	return sequence.size();
}


bool ofxImageSequence::isLoaded(){						//returns true if the sequence has been loaded
    return loaded;
}

bool ofxImageSequence::isLoading(){
	return loading;
}
