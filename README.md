# A UTF-7 stream encoder and decoder in ANSI C

This is small, free-standing, public domain library encodes a stream of
code points into UTF-7 and vice versa. It requires as little as 32 bytes
of memory for its internal state.

Initialize a context struct (`struct utf7`), set its `buf` and `len`
pointers to a buffer for input or output, then "pump" the encoder or
decoder similarly to zlib.

```c
char buffer[256];
struct utf7 ctx;

utf7_init(&ctx, "=@"); /* indirectly encode = and @ */
ctx.buf = buffer;
ctx.len = sizeof(buffer));
```

The context must be re-initialized before switching between encoding and
decoding.

## `utf7_init()`

```c
void utf7_init(struct utf7 *, const char *indirect);
```

The `utf7_init()` function initializes a context for either encoding
or decoding. The `indirect` argument is optional and may be NULL. It
is ignored when the context is used for decoding. By default the
encoder directly encodes every character that is permitted to be
directly encoded. The `indirect` argument subtracts from this set of
directly-encoded characters. This may be desirable for certain
characters, such as `=` (EQUALS SIGN).

## `utf7_encode()`

```c
int utf7_encode(struct utf7 *, long codepoint);
```

The `utf7_encode()` function writes a code point to the buffer pointed
to by the context. The `buf` and `len` fields are updated on the
context as output is produced. Code points outside the Basic
Multilingual Plane (BMP) are automatically encoded as surrogate halves
for UTF-7.

When there is nothing more to encode, call the encoder with the
special `UTF7_FLUSH` code point to force all remaining output from the
context. This behaves just like any other code point, particularly
with respect to the return values below, but obviously this value will
not be written into the output. After flushing, the context will
effectively be reinitialized.

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
context and returning a code point. Code points outside the BMP are
left as surrogate halves, so it is up to the caller to re-assemble
them if necessary.

There are four possible return values:

* `UTF7_OK`: Input was exhausted, but this is a valid ending for a
  stream.

* `UTF7_INCOMPLETE`: Input was exhausted but more input is expected. If
  there is no more input, this should be treated as an error, since the
  input was truncated.

* `UTF7_INVALID`: The input is not valid UTF-7. The offending byte lies
  one byte before the `buf` pointer.

* Any other return value is a code point.
