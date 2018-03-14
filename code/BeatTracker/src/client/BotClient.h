/*
 * BotClient.h
 *
 *  Created on: Mar 14, 2018
 *      Author: JochenAlt
 */

#ifndef SRC_CLIENT_BOTCLIENT_H_
#define SRC_CLIENT_BOTCLIENT_H_

#include <Move.h>
#include "restclient/restclient.h"
#include "restclient/connection.h"


class BotClient {
public:
	BotClient();
	virtual ~BotClient();

	// return singleton
	static BotClient& getInstance();

	// is webclient running?
	bool isActive() { return isWebClientActive; };

	// setup rest connection to server
	void setup(string host);

	// fetch current state of the bot
	void getStatus();

	Pose getHeadPose() { return headPose; };
	Pose getBodyPose() { return bodyPose; };
	double getAmbition() { return ambition; };
	Move::MoveType getMove() { return move; };

private:
	RestClient::Connection* webserverConnection = NULL;
	Pose bodyPose;
	Pose headPose;
	float ambition = 0;
	Move::MoveType move = Move::MoveType::NO_MOVE;
	bool isWebClientActive = false;
};

#endif /* SRC_CLIENT_BOTCLIENT_H_ */