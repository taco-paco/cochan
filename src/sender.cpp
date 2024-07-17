//
// Created by Edwin Paco on 7/12/24.
//

#include <co_chan/sender.hpp>

// TODO: be careful with double locking in resume. check...
// try send
// if can just push

/// Design discussion:
// Hypothesis: check receivers only when queue.size() == 0 ?
// what pending receiver means: noting to receive and I'm suspeneded

/// Multiple producers single receiver
// At moment there's single receiver waiting.
// That means it was suspended when queue.size() == 0
// So first sender to come would wake him up
// So it suffices for sender to check receiver only when queue.size() == 0

/// Multiple producers multiple receivers
// so 2 receivers pending
// still: That means they were suspended when queue.size() == 0
// But we can't check only queue.size() == 0
// The reason is that if there 2 of them and we wake up first during queues.size() == 0
// We add el to queue schedule/resume receiver to pick it up
// In the meanwhile another send occurs and queue.size() == 1 at this point
// Thus we don't wake up second receiver

// Way to resolve the above is:
// when scheduling receiver just give hive directly the el-t that is being sent.
// This way queue size doesn't increase

/// Receiver picks up el-t
// same argument for queue.size() == capacity ?
// Means senders hod no one to send to and got suspended
// If queue.size() < capacity seems no sense to check. How there could be pending sender?
// What if 2 senders pending. Receiver schedules one. in mean time new receiver comes faster than sender scheduled
// so queue.size() < capacity and 2 sender isn't woken uo

/// Solution(?):
// std::list<std::pair<Value, std::coroutine_handle>> senderWaiters;
// so when receiver schedules sender it just push Value in queue himself
// Is that fucking bad idea or not? Mixes up responsibilities??
// Like we add el-t to queue and then wake up sender so it kinda doesn't have to do shit.

// Seemd like receiver kinda populates array at the same time. And can pause itself in a sense.
// Does this shit makes sense only for sender?? Sooo while receiver as result can pause itself
// sender won't pause itself if it will give

/// Sooo what's the design then
// When we park receiver we allocation spot were sender can move his value. Can we do it via resume?
// sender checks only queue.size() == 0;
// receiver all the time or it will park itself

/// Problem
// Even if receiver checks queue.size() == cap only and wakes up sender
// another sender can come write in available slot
// so receiver have to take sender from quueue and push his value

// can push:
//  1. push
//  2. maybe waiting receivers
//  3, check and schedule/resume if so

// can't push
// 1. susppend

// Lemma: senders may be present only when channel.size == capacity
// Assume not. Since senders are added to list onlu when channel is full.
// That means that receiver that read the value didn't pop waiting sender

// id we can push there maybe waiting receivers
Sender::Sender( channel* theChan )
    : chan( theChan )
{
}

Sender::Sender( const Sender& sender )
{
    this->chan = sender.chan;
}

AwaitableSend Sender::send( int value )
{
    return AwaitableSend{ value, chan };
}
