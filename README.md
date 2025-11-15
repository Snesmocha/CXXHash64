# CXXHash64
a bare bones simd accelerated, cross compiler compatible header only xxhash64 implementation written in c99


the implementation in -O3 -flto is around 2gb/s faster than xxhash3 under nonidealistic circumstances. Around 5x faster than the c++ standard hasher as well for both small data and large data.


what this is for: someone who wants good enough practical performance on their hash implementation without needing a horrendously complex system or a massive header import

what this isn't for: someone whose looking for maximum "theoredical performance" 
