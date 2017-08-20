#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"

#include "Message\Json.h"

#include "LinkMessage.h"

#include "Security\RC4.h"

void LinkMessage::Init() {
	*(byte*)Reply = 0;
	Seq = 0;
	Code = 0;
	Length = 0;
}

void LinkMessage::Show(bool newline) const {
	String str((cstring)&this[1], Length);
	str.Show(newline);
}
