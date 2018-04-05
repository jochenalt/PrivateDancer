/*
 * AudioProcessor.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Jochen Alt
 */

#include <audio/AudioFile.h>
#include <iostream>
#include <string.h>

#include <stdlib.h>
#include <signal.h>
#include <chrono>
#include <unistd.h>
#include <iomanip>
#include <thread>
#include <string>
#include <unistd.h>
#include <iomanip>
#include <algorithm>

#include <basics/stringhelper.h>
#include <basics/util.h>
#include <audio/AudioProcessor.h>
#include <audio/MicrophoneInput.h>
#include <audio/Playback.h>

#include <dance/RhythmDetector.h>
#include <dance/Dancer.h>

#include <beat/BTrack.h>

using namespace std;

AudioProcessor::AudioProcessor() {
	beatCallback = NULL;
}

AudioProcessor::~AudioProcessor() {
}

void AudioProcessor::setup(BeatCallbackFct newBeatCallback) {
    beatCallback = newBeatCallback;
	inputAudioDetected = false;
	// playback and microphone are setup when started

	// music detection requires 1s of music before flagging it as music
	beatScoreFilter.init(100);
}

void AudioProcessor::setVolume(double newVolume) {
	volume = newVolume;
}
double AudioProcessor::getVolume() {
	return volume;
}

void AudioProcessor::setPlayback(bool ok) {
	playback.setPlayback(ok);
}

bool AudioProcessor::getPlayback() {
	return playback.getPlayback();
}

void AudioProcessor::setWavContent(std::vector<uint8_t>& newWavData) {
	// set new data and indicate to change the source
	nextWavContent = newWavData;

	// indicate that current processing is to be stopped
	stopCurrProcessing = true;

	nextInputType = WAV_INPUT;
}

void AudioProcessor::setAudioSource() {

	if (nextInputType == WAV_INPUT) {
		// likely a new rythm
		RhythmDetector::getInstance().setup();

		// current input is wav data
		currentInputType = WAV_INPUT;

		// reset position in wav content to start
		wavInputPosition = 0;

		beatScoreFilter.set(0);

		// re-initialize dancing when new input source is detected
		Dancer::getInstance().setup();

		// read in the wav data and set index pointer to first position
		currentWavContent.decodeWaveFile(nextWavContent);

		// playback is done with same sample rate like the input wav data
		playback.setup(currentWavContent.getSampleRate());

		// clear input, has been saved
		nextWavContent.clear();

		cout << "switching audio source to wav input" << endl;
	}
	if (nextInputType == MICROPHONE_INPUT) {
		currentInputType = MICROPHONE_INPUT;

		// playback is set to standard sample rate
		playback.setup(MicrophoneSampleRate);

		// initialize the
		microphone.setup(MicrophoneSampleRate);

		cout << "switching to microphone input" << endl;
	}

	// do not switch source again until explicitely set
	nextInputType = NO_CHANGE;

}
void AudioProcessor::setMicrophoneInput() {
	// flag that processing loop should stop
	stopCurrProcessing = true;

	// current input is microphone
	nextInputType = MICROPHONE_INPUT;
}

int AudioProcessor::readWavInput(double buffer[], unsigned BufferSize) {
	int numSamples = currentWavContent.getNumSamplesPerChannel();
	int numInputSamples = min((int)BufferSize, (int)numSamples-wavInputPosition);
	int numInputChannels = currentWavContent.getNumChannels();

	int bufferCount = 0;
	for (int i = 0; i < numInputSamples; i++)
	{
		double inputSampleValue = 0;
		inputSampleValue= currentWavContent.samples[0][wavInputPosition + i];
		assert(wavInputPosition+1 < numSamples);
		switch (numInputChannels) {
		case 1:
			inputSampleValue= currentWavContent.samples[0][wavInputPosition + i];
			break;
		case 2:
			inputSampleValue = (currentWavContent.samples[0][wavInputPosition + i]+currentWavContent.samples[1][wavInputPosition + i])/2;
			break;
		default:
			inputSampleValue = 0;
			for (int j = 0;j<numInputChannels;j++)
				inputSampleValue += currentWavContent.samples[j][wavInputPosition + i];
			inputSampleValue = inputSampleValue / numInputChannels;
		}
		buffer[bufferCount++] = inputSampleValue;
	}
	wavInputPosition += bufferCount;
	return bufferCount;
}

void AudioProcessor::processInput() {
	// reset flag that would stop the loop (is set from the outside)
	stopCurrProcessing = false;

	// hop size is the number of samples that will be fed into beat detection
	const int hopSize = 256; // approx. 3ms at 44100Hz

	// number of samples to be read
	const int numInputSamples = hopSize;

	// framesize is the number of samples that will be considered in this loop
	// cpu load goes up linear with the framesize
	int frameSize = hopSize*8;

	// initialize beat detector
	BTrack beatDetector(hopSize, frameSize);

	// start time used for delays and output
	uint32_t startTime_ms = millis();

	// buffer for audio coming from wav or microphone
	double inputBuffer[numInputSamples];
	int inputBufferSamples  = 0;

	int sampleRate = 0;
	while (!stopCurrProcessing) {
		if (currentInputType == MICROPHONE_INPUT) {
			inputBufferSamples = microphone.readMicrophoneInput(inputBuffer, numInputSamples);
			sampleRate = MicrophoneSampleRate;
		}
		if (currentInputType == WAV_INPUT) {
			inputBufferSamples = readWavInput(inputBuffer, numInputSamples);

			sampleRate = currentWavContent.getSampleRate();
			if (inputBufferSamples < numInputSamples) {
				cout << "end of song. Switching to microphone." << endl;
				beatDetector.initialise(hopSize, frameSize);

				// use microphone instead of wav input
				setMicrophoneInput();
				setAudioSource();
			}
		}

		// play the buffer of hopSize asynchronously
		playback.play(volume, inputBuffer,numInputSamples);

		// detect beat and bpm of that hop size
		beatDetector.processAudioFrame(inputBuffer);
		bool beat = beatDetector.beatDueInCurrentFrame();
		double bpm = beatDetector.getCurrentTempoEstimate();

		if (beat){
			cout << std::fixed << std::setprecision(2) << "Beat (" << beatDetector.getCurrentTempoEstimate() << ")"  << endl;
	    };

		// check if the signal is really music. low pass scoring to ensure that small pauses are not
		// misinterpreted as end of music
		double score = beatDetector.getLatestCumulativeScoreValue();
		beatScoreFilter.set(score);
		const double scoreThreshold = 10.;
		inputAudioDetected = (beatScoreFilter >= scoreThreshold);

		processedTime = millis()/1000.0;
		beatCallback(beat, bpm);

		if (currentInputType == WAV_INPUT) {
			// insert a delay to synchronize played audio and beat detection before entering the next cycle
			double elapsedTime = ((double)(millis() - startTime_ms)) / 1000.0f;  	// [s]
			double processedTime = (double)wavInputPosition / (double)sampleRate;	// [s]
			// wait such that elapsed time and processed time is synchronized
			double timeAhead_ms = (processedTime - elapsedTime)*1000.0;
			if (timeAhead_ms > 1.0) {
				delay_ms(timeAhead_ms);
			}
		}
	}
	// check if the source needs to be changed
	setAudioSource();
}

float AudioProcessor::getLatency() {
	if (currentInputType == MICROPHONE_INPUT)
		return microphone.getMicrophoneLatency();
	else
		return 0.5;
}

