#include "Elcano_Serial.h"

/*
** Elcano_Serial.cpp
** By Dylan Katz
**
** Manages our protocal for robust communication of SerialData over serial connections
*/

int8_t ParseState::update(void) {
	int c = dev->read();
	if (c == -1) return PSE_UNAVAILABLE;
	if (c == ' ' || c == '\t' || c == '\0' || c == '\r') return PSE_INCOMPLETE;
	switch(state) {
	case 0:
		// During this state, we begin the processing of a new SerialData
		// packet and we must receive {D,S,G,X} to state which type of packet
		// we are working with
		dt->clear();
		switch(c) {
		case 'D': dt->kind = MSG_DRIVE;  break;
		case 'S': dt->kind = MSG_SENSOR; break;
		case 'G': dt->kind = MSG_GOAL;   break;
		case 'X': dt->kind = MSG_SEG;    break;
		default : return PSE_BAD_TYPE;
		}
		state = 1;
		return PSE_INCOMPLETE;
	case 1:
		// During this state, we need to find '{' if we are reading the value
		// for an attribute, or '\n' if we are done with the packet
		switch(c) {
		case '\n': state = 0; return dt->verify() ? PSE_SUCCESS : PSE_INVAL_COMB;
		case '{' : state = 2; return PSE_INCOMPLETE;
		default  : return PSE_BAD_LCURLY;
		}
	case 2:
		// During this state, we begin reading an attribute of the SerialData
		// and we must recieve {n,s,a,b,r,p} to state _which_ attribute
		switch(c) {
		case 'n': state = 3; dt->number      = 0; return PSE_INCOMPLETE;
		case 's': state = 4; dt->speed_cmPs  = 0; return PSE_INCOMPLETE;
		case 'a': state = 5; dt->angle_deg   = 0; return PSE_INCOMPLETE;
		case 'b': state = 6; dt->bearing_deg = 0; return PSE_INCOMPLETE;
		case 'r': state = 7; dt->probability = 0; return PSE_INCOMPLETE;
		case 'p': state = 8; dt->posE_cm = 0; dt->posN_cm = 0; return PSE_INCOMPLETE;
		default : return PSE_BAD_ATTRIB;
		}
#define STATES(SS, PS, NS, TERM, HOME, VAR) \
    case SS:                                \
        if (c == '-')              {        \
            state = NS;                     \
            return PSE_INCOMPLETE; }        \
        state = PS;                         \
    case PS:                                \
        if (c >= '0' && c <= '9')  {        \
            dt->VAR *= 10;                  \
            dt->VAR += c - '0';             \
            return PSE_INCOMPLETE; }        \
        else if (c == TERM)        {        \
            state = HOME;                   \
            return PSE_INCOMPLETE; }        \
        else                                \
            return PSE_BAD_NUMBER;          \
    case NS:                                \
        if (c >= '0' && c <= '9')  {        \
            dt->VAR *= 10;                  \
            dt->VAR -= c - '0';             \
            return PSE_INCOMPLETE; }        \
        else if (c == TERM)        {        \
            state = HOME;                   \
            return PSE_INCOMPLETE; }        \
        else                                \
            return PSE_BAD_NUMBER;
STATES(3, 13, 23, '}', 1, number)
STATES(4, 14, 24, '}', 1, speed_cmPs)
STATES(5, 15, 25, '}', 1, angle_deg)
STATES(6, 16, 26, '}', 1, bearing_deg)
STATES(7, 17, 27, '}', 1, probability)
STATES(8, 18, 28, ',', 9, posE_cm)
STATES(9, 19, 29, '}', 1, posN_cm)
#undef STATES
	}
}

bool SerialData::write(HardwareSerial *dev) {
	switch (kind) {
	case MSG_DRIVE:  dev->print("D"); break;
	case MSG_SENSOR: dev->print("S"); break;
	case MSG_GOAL:   dev->print("G"); break;
	case MSG_SEG:    dev->print("X"); break;
	default:         return false;
	}
	if (number != NaN && (kind == MSG_GOAL || kind == MSG_SEG)) {
		dev->print("{n ");
		dev->print(number);
		dev->print("}");
	}
	if (speed_cmPs != NaN && kind != MSG_GOAL) {
		dev->print("{s ");
		dev->print(speed_cmPs);
		dev->print("}");
	}
	if (angle_deg != NaN && (kind == MSG_DRIVE || kind == MSG_SENSOR)) {
		dev->print("{a ");
		dev->print(angle_deg);
		dev->print("}");
	}
	if (bearing_deg != NaN && kind != MSG_DRIVE) {
		dev->print("{b ");
		dev->print(bearing_deg);
		dev->print("}");
	}
	if (posE_cm != NaN && posN_cm != NaN && kind != MSG_DRIVE) {
		dev->print("{p ");
		dev->print(posE_cm);
		dev->print(",");
		dev->print(posN_cm);
		dev->print("}");
	}
	if (probability != NaN && kind == MSG_GOAL) {
		dev->print("{r ");
		dev->print(probability);
		dev->print("}");
	}
	dev->print("\n");
	return true;
}

void SerialData::clear(void) {
    kind = MSG_NONE;
    number = NaN;
    speed_cmPs = NaN;
    angle_deg = NaN;
    bearing_deg = NaN;
    posE_cm = NaN;
    posN_cm = NaN;
    probability = NaN;
}

bool SerialData::verify(void) {
	switch (kind) {
	case MSG_DRIVE:
		if (speed_cmPs == NaN) return false;
		if (angle_deg  == NaN) return false;
		break;
	case MSG_SENSOR:
		if (speed_cmPs == NaN)  return false;
		if (posE_cm == NaN)     return false;
		if (posN_cm == NaN)     return false;
		if (bearing_deg == NaN) return false;
		if (angle_deg == NaN)   return false;
		break;
	case MSG_GOAL:
		if (number == NaN)      return false;
		if (posE_cm == NaN)     return false;
		if (posN_cm == NaN)     return false;
		if (bearing_deg == NaN) return false;
		break;
	case MSG_SEG:
		if (number == NaN)      return false;
		if (posE_cm == NaN)     return false;
		if (posN_cm == NaN)     return false;
		if (bearing_deg == NaN) return false;
		if (speed_cmPs == NaN)  return false;
		break;
	default:
		return false;
	}
	return true;
}
