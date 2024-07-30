# cochan

This library provides C++20 implementation of channels using coroutines.
Design inspired by Go & Rust

### Closing channel

Channel can be explicitly closed via `Receiver::close` call. It is also closed implicitly
once whether all "sendable"s or "receivable"s are destructed. <br />
"sendable": `Sender` or `AwaitableSend` <br />
"receivable": `Receiver` or `AwaitableReceive`

### Permits

`AwaitableSend` works as permit for sending. If `AwaitableSend` was given by the `Sender` before channel is closed,
you can send your message even after the channel is closed by Receiver side.

If Receiver side is closed due to destruction of all "receivables" the message will be sent into nothing.
If Receiver::closed explicitly, message will be sent and can be consumed until we don't run out of permits.

### Lifetime of channel

The channel is destructed by last entity from sendables and receivables.