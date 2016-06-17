#include "Type.h"
#include "DateTime.h"
#include "SString.h"

#include "EventHandler.h"

/************************************************ EventHandler ************************************************/
EventHandler::EventHandler()
{
	Method	= nullptr;
	Target	= nullptr;
}

EventHandler& EventHandler::operator=(const Func& func)
{
	Method	= (void*)func;

	return *this;
}

void EventHandler::Add(Func func)
{
	Method	= (void*)func;
}

void EventHandler::operator()()
{
	if(!Method) return;
	
	if(Target)
		((Action)Method)(Target);
	else
		((Func)Method)();
}

/*void EventHandler::Invoke(void* arg)
{
	if(!Method) return;
	
	auto func	= (Func)Method;
	
	if(Target)
		func(Target, arg);
	else
		((Action)Method)(arg);
}

void EventHandler::Invoke(void* arg1, void* arg2)
{
	if(!Method) return;
	
	auto func	= (Func)Method;
	
	if(Target)
		func(Target, arg1, arg2);
	else
		func(arg1, arg2);
}*/
