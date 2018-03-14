/*
 * MoveMaker.cpp
 *
 *  Created on: Feb 13, 2018
 *      Author: jochenalt
 */


#include <Dancer.h>
#include "basics/util.h"

#include "RhythmDetector.h"
#include "Stewart/BodyKinematics.h"


Dancer ::Dancer () {
}

Dancer ::~Dancer () {
}

Dancer & Dancer ::getInstance() {
	static Dancer  mm;
	return mm;
}


Pose Dancer ::getDefaultBodyPose() {
	return Pose(Point(0,0,bodyHeight), Rotation (0,0,0));
}

Pose Dancer ::getDefaultHeadPose() {
	return Pose(Point(0,0,headHeight), Rotation (0,0,0));
}

void Dancer ::setup() {
	pose.body = getDefaultBodyPose();
	pose.head = getDefaultHeadPose();

	currentMove = Move::NO_MOVE;
	passedBeatsInCurrentMove = 0;
	startAfterNBeats = 4;
}

void Dancer ::createMove(double movePercentage) {
	TotalBodyPose newPose = Move::getMove(currentMove).move(movePercentage);
	pose = newPose;
}

void Dancer::setDanceParameters(Move::MoveType newCurrentMove, double newAmbition, const Pose& newBodyPose, const Pose& newHeadPose ) {
	currentMove = newCurrentMove;
	ambition = newAmbition;
	pose.body = newBodyPose;
	pose.head = newHeadPose;
}

void Dancer ::danceLoop(bool beat, double BPM) {
	if (beat) {

		// first move is the classical head nicker
		if (RhythmDetector::getInstance().isFirstBeat())
			currentMove = (Move::MoveType)(0);

		// switch to next move after as soon as rhythm starts again
		passedBeatsInCurrentMove++;
		if ((sequenceMode == AUTOMATIC_SEQUENCE) &&
			(RhythmDetector::getInstance().getAbsoluteBeatCount() > startAfterNBeats) &&
			(RhythmDetector::getInstance().hasBeatStarted()) &&
			(passedBeatsInCurrentMove*RhythmDetector::getInstance().getRhythmInQuarters() >= Move::getMove(currentMove).getLength()) &&
			(RhythmDetector::getInstance().getBeatCount() == 0)) {
			doNewMove();
			passedBeatsInCurrentMove = 0;
		}
	}

	// wait 4 beats to detect the rhythm
	if ((RhythmDetector::getInstance().getAbsoluteBeatCount() > startAfterNBeats) && (RhythmDetector::getInstance().hasBeatStarted())) {
		createMove(RhythmDetector::getInstance().getRythmPercentage());
	}
}

void Dancer ::doNewMove() {
	// when all moves are shown, omit plain headnicker
	if ((int)currentMove >= Move::numMoves()-1)
		currentMove = (Move::MoveType)1; // don't restart with physicists move
	else
		currentMove = (Move::MoveType) (((int)currentMove + 1));

	BodyKinematics::getInstance().resetSpeedMeasurement();
	cout << "new move: " << Move::getMove(currentMove).getName() << "(" << (int)currentMove << ")" << endl;
}

void Dancer ::setCurrentMove(Move::MoveType m) {
	currentMove = m;
	passedBeatsInCurrentMove = 0;
}
