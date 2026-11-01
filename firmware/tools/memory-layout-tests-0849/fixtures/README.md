The two 640-byte SHA-mbles collision fixtures come from the upstream
SHA-1 collision detection project (MIT license):

https://github.com/cr-marcstevens/sha1collisiondetection/tree/a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988/test

`run_static.py` verifies their pinned SHA-256 digests before use, verifies their
matching ordinary SHA-1 values, and requires the production collision detector
to flag both inputs and change their digest when safe hashing is enabled.
These are test data and are never installed on the SD card.
