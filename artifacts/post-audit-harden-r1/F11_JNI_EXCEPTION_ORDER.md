# F-11 — `touchEvent` exception ordering

## Before (baseline `forgeshape_jni.cpp` 5228–5234)

```
env->GetIntArrayRegion(ids, 0, count, idBuf);
env->GetFloatArrayRegion(xs, 0, count, xBuf);
env->GetFloatArrayRegion(ys, 0, count, yBuf);
if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
```

A short `ids` array raises `ArrayIndexOutOfBoundsException` on the first read
and the next two JNI calls run with it pending, which the JNI specification
forbids and CheckJNI aborts on.

## After

Each region read is followed by its own `ExceptionCheck`; on a pending
exception it is cleared and the event is dropped BEFORE any further JNI call,
exactly what the single check used to do for the whole block. The three optional
stylus arrays already went through `readOptional*Region`, which had the right
order. One four-line comment states the rule at the site.

Product-visible behaviour is preserved on purpose: a malformed required array
still drops that one event and clears the exception, as it has since the touch
boundary was written (`ForgeShapeSurfaceView` always passes six-slot arrays, so
the product never reaches it). The exception is not swallowed to make a test
green — the drop was the existing contract; only the ORDER changed.

## Evidence

* **RED** — the new test, run against the PRE-FIX JNI (the rest of the tree
  already carried this stage's other changes), crashed the process under
  CheckJNI, which is on for a debuggable app on the emulator
  (`FOCUSED_RED_PREFIX_JNI.txt`: `INSTRUMENTATION_RESULT: shortMsg=Process
  crashed.`). The abort message (`F11_RED_CRASH_LOGCAT.txt`):

  ```
  JNI DETECTED ERROR IN APPLICATION: JNI GetFloatArrayRegion called with
  pending exception java.lang.ArrayIndexOutOfBoundsException: int[] offset=0
  length=2 src.length=1
    at void com.forgeshape.app.NativeViewport.touchEvent(...)
    at ...JniBoundaryHardeningTest.lambda$touchEventDropsShortRequiredArraysWithoutPendingException...
  ```

* **GREEN** — the same test on the fixed JNI (`FOCUSED_GREEN.txt`,
  `OK (5 tests)`): the short-`ids`, short-`xs` and short-`ys` events are each
  dropped with the previous event still on record (`debugLastPointerEvent`),
  a well-formed event afterwards is recorded, and every call returns to Java
  with no exception (PAH-R1-12, PAH-R1-13).
* Nothing leaks on any branch: the region reads copy into stack buffers, and
  no local reference or critical section is taken before the checks.
