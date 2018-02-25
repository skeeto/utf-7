# A UTF-7 stream encoder and decoder in ANSI C

This is small, free-standing, public domain library encodes a stream of
code points into UTF-7 and vice versa. It requires as little as 16 bytes
of memory for its internal state.

Your job is to zero-initialize (`UTF7_INIT`) a context struct (`struct
utf7`), set its `buf` and `len` pointers to a buffer for input or
output, then "pump" the encoder or decoder similarly to zlib.

```c
char buffer[256];
struct utf7 ctx = UTF7_INIT(buffer, sizeof(buffer));
```

The context must be re-initialized before switching between encoding and
decoding.

## `utf7_encode()`

```c
int utf7_encode(struct utf7 *, long codepoint, unsigned flags);
```

This function writes the given code point to the buffer pointed to by
the context. The encoder uses a *direct* encoding for all *optionally
direct* code points by default. Using the `UTF7_INDIRECT` flag overrides
this behavior for the given code point and forces it to be indirectly
encoded (e.g. base64). This may be desirable for certain characters,
such as `=` (EQUALS SIGN).

The `buf` and `len` fields are updated as output is written to the
output buffer. Code points outside the Basic Multilingual Plane (BMP)
are automatically encoded into surrogate halves for UTF-7.

There are two possible return values:

* `UTF7_OK`: The operation completed successfully.

* `UTF7_FULL`: The output buffer filled up before the operation could be
  completed. Consume the output buffer as appropriate for your
  application, update the context's `buf` and `len` to a fresh buffer,
  and continue the operation by calling it again with the exact same
  arguments.

## `utf7_decode()`

```c
long utf7_decode(struct utf7 *);
```

This function operates in reverse, consuming input from `buf` on the
context and returning a code point. Code points outside the BMP are left
as surrogate halves, so it is up to the caller to re-assemble them.

There are four possible return values:

* `UTF7_OK`: Input was exhausted, but this is a valid ending for a
  stream.

* `UTF7_INCOMPLETE`: Input was exhausted but more input is expected. If
  there is no more input, this should be treated as an error, since the
  input was truncated.

* `UTF7_INVALID`: The input is not valid UTF-7. The offending byte lies
  one byte before the `buf` pointer.

* Any other return value is a code point.
