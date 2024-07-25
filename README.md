# cochan

This library provides C++20 implementation of channels using coroutines.
Design inspired by Go & Rust

### Closing channel

Channel can be explicitly closed via `Receiver::close` call. It is also closed implicitly
once whether all "sendable"s or "receivable"s are destructed. <br />
"sendable": `Sender` or `AwaitableSend` <br />
"receivable": `Receiver` or `AwaitableReceive`

### Lifetime of channel

The channel is destructed by last entity from sendables and receivables.