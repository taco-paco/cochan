This is file is to explain the logic of implementation, and also reminder for me.

### Workflow of `bool channel::handleSend`

If `sendQueue` is full(reached `capacity`) then we do one of the following:

1. If there are no receivables we discard the value and return control to the caller. (*)
2. Push coroutine handle into `senderWaiters` and park coroutine.

After we check if

```c++
if( !sendQueue.empty() )
```

1. In case has el-ts push our value into `sendQueue` and let coroutine continue.
2. Empty `sendQueue` means that there could be parked receivers within receiverWaiters.

In 2 case we check:

```c++
if( !receiverWaiters.empty() )
```

1. If true. We take first parked receiver and wake it up with our "value"
2. If false. Move value into `senqQueue` and continue execution.

### Workflow of `channel::handleReceive`

Check

```c++
if( sendQueue.empty() )
```

1. If true. Nothing to receive. `if( ( senders == 0 || closed ) && awaitableSenders == 0 )` we return `std::nullopt`
   right away, since no one will send anything.
   Otherwise push coroutine into `receiverWaiters` and park it.
2. If false. Take value from `sendQueue`.
   After we take value we will return it to `Receiver` but first we need to check if there're waiting senders that we
   can wake up.
   Notice that this can only be the case if `sendQueue` was full prior to our `pop`. (**)

## Argument

Logic above relies on the following argument:

```c++
if( sendQueue.empty() )
{
    // ...
}

ASSERT( receiverWaiters.empty(), "If element's in queue receiverWaiters shall be empty" )
```

Statement: If `sendQueue` isn't empty then `receiverWaiters` is, and other way around.

My logic: Assume incorrect and without loss of generality there's 1 el-t in `sendQueue`
and 1 "parked" coroutine in `receiverWaiters`. Those `handleSend` and `handleReceive` operations are made atomic
by `mutex` with respect to each
other. This means that when `sendQueue` was populated there already was value in `receiverWaiters`, and other way
around.
Let's review first: if that is the case, then according to implementation we should wake up receiver
from `receiverWaiters`. Leaving it with size 0.
We get contradiction with our assumption. The same is other way around. If there's value in sendQueue, receiver would
just pick it up.

### Traits of thesis

(*) Above allows us do the following checks.

```c++
if( sendQueue.size() == capacity )
{
    if( receivers == 0 && awaitableReceivers == 0 )
    {
        
    }
    
    // ...
}
```

The inner check should be really

```c++
if( receivers == 0 && (awaitableReceivers - receiverWaiters.size()) == 0 )
```

since we interested if there're awaitables that exist but were not triggered yet.
But because we're in case `sendQueue.size() > 0` this means that `receiverWaiters.size() == 0`, hence getting existing
check.

Same for

```c++
if( ( senders == 0 || closed ) && awaitableSenders == 0 )
```

only difference is we check if channel was closed. awaitableSenders checked separately
since if it was created it means that back then channel wasn't closed, so design choice is to let them send.

Statement (**): if `senderWaiters` not empty - sendQueue is full.

P.S really hope above is correct. reach out if it breaks somewhere.

### Not important. Preserving my garbage thoughts.

```c++
// TODO receivePermits shall get res as well
// If sendq.empty() && senders == 0  && sendersPermit == 0
// return std::nullopt
// If sendq.empty() && senders == 0  && sendersPermit != 0
// park

// But will it be awakened?
// So parked ones will be awakened by sendPermitted(awaitables) directly
// remnants will be awakened by onSenderClose.
// If after that receiverPermit triggered it will fall under 1st branch above
```

```c++
if( sendQueue.empty() )
{
    // No one will send anything already
    // -----
    // This is not working if --senderPermits in destructor
    // Cause all of them can be suspended. so we will park
    
    // if( senders == 0 && ( sendAwaiters - senderWaiters.size() ) == 0 )
    // senderWaiters shall be empty. Optimization found?
    if( ( senders == 0 || closed ) && awaitableSenders == 0 )
    {
        *receiver.first = std::nullopt;
        return false;
    }
    
    // So here the idea is:
    // If we have senders and not triggered permits.
    // Then we can park because they will wake us up.
    // This logic above won't work if
    
    // Nothing to receive - park
    // Senders or AwaitableSends will wake up everyone eventually
    // By handleSend or destruct logic
    receiverWaiters.push_back( receiver );
    return true;
}
```

```c++
// TODO: if last receiver call. Wake up all senders
// replace with: if (receivers == 0 && (awaiters - receiverWaiter.size())) == 0)
// but if sendQueue.size() != 0, receiverWaiter.size() == 0, so
// replace with (receivers == 0 && awaiters == 1) (the last one)
//        if( receivers == 0 && awaitableReceivers == 0 )
//        {
//            wake up senders(moved to destructor of Receiver/AwaitableReceive)
//        }

*receiver.first = std::move( value );
```