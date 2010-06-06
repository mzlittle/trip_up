// forkjoin.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "forkjoin.h"
#include "queue.h"
#include <memory>

Fork::Fork(int n, BufferedTransformation *const *givenOutPorts)
	: numberOfPorts(n), outPorts(n)
{
	currentPort = 0;

	for (unsigned int i=0; i<numberOfPorts; i++)
		outPorts[i].reset(givenOutPorts ? givenOutPorts[i] : new ByteQueue);
}

void Fork::SelectOutPort(int portNumber)
{
	currentPort = portNumber;
}

void Fork::Detach(BufferedTransformation *newOut)
{
	std::auto_ptr<BufferedTransformation> out(newOut ? newOut : new ByteQueue);
	outPorts[currentPort]->Close();
	outPorts[currentPort]->TransferTo(*out);
	outPorts[currentPort].reset(out.release());
}

void Fork::Attach(BufferedTransformation *newOut)
{
	if (outPorts[currentPort]->Attachable())
		outPorts[currentPort]->Attach(newOut);
	else
		Detach(newOut);
}

void Fork::Close()
{
	InputFinished();

	for (unsigned int i=0; i<numberOfPorts; i++)
		outPorts[i]->Close();
}

void Fork::Put(byte inByte)
{
	for (unsigned int i=0; i<numberOfPorts; i++)
		outPorts[i]->Put(inByte);
}

void Fork::Put(const byte *inString, unsigned int length)
{
	for (unsigned int i=0; i<numberOfPorts; i++)
		outPorts[i]->Put(inString, length);
}

// ********************************************************

Join::Join(unsigned int n, BufferedTransformation *outQ)
	: Filter(outQ),
	  numberOfPorts(n),
	  inPorts(n),
	  interfacesOpen(n),
	  interfaces(n)
{
	for (unsigned int i=0; i<numberOfPorts; i++)
	{
		inPorts[i].reset(new ByteQueue);
		interfaces[i].reset(new Interface(*this, *inPorts[i], i));
	}
}

Interface * Join::ReleaseInterface(unsigned int i)
{
	return interfaces[i].release();
}

void Join::NotifyInput(unsigned int i, unsigned int /* length */)
{
	AccessPort(i).TransferTo(*outQueue);
}

void Join::NotifyClose(unsigned int /* id */)
{
	if ((--interfacesOpen) == 0)
		outQueue->Close();
}

// ********************************************************

void Interface::Put(byte inByte)
{
	bq.Put(inByte);
	parent.NotifyInput(id, 1);
}

void Interface::Put(const byte *inString, unsigned int length)
{
	bq.Put(inString, length);
	parent.NotifyInput(id, length);
}

unsigned long Interface::MaxRetrieveable() 
{
	return parent.MaxRetrieveable();
}

void Interface::Close() 
{
	parent.NotifyClose(id);
}

void Interface::Detach(BufferedTransformation *bt) 
{
	parent.Detach(bt);
}

void Interface::Attach(BufferedTransformation *bt) 
{
	parent.Attach(bt);
}

unsigned int Interface::Get(byte &outByte) 
{
	return parent.Get(outByte);
}

unsigned int Interface::Get(byte *outString, unsigned int getMax)
{
	return parent.Get(outString, getMax);
}

unsigned int Interface::Peek(byte &outByte) const
{
	return parent.Peek(outByte);
}
