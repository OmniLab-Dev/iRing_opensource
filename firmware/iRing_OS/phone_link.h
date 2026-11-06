#pragma once
#include <stdint.h>
#include "media_queue.h"
enum PhoneStatus { PHONE_DISCONNECTED, PHONE_PAIRING, PHONE_READY };
bool phoneBegin(); // Call after Bluefruit.begin(), before advertising.
PhoneStatus phoneStatus();
uint32_t phoneGeneration();
bool phoneSend(MediaKey key);
uint16_t phonePoll(uint32_t now); // Returns the successfully submitted press.
