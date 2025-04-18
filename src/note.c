#include "note.h"

void buildKeysFrom(NBSTickEffective *t, u16 *keyDown, u16 *keyUp) {
  NBSNoteBlock *note;
  *keyDown = *keyUp = 0;
  for (int i = 0; i < t->noteCtr; i++) {
    vec_at(&t->notes, i, (void **)&note);
    if (note->instrument == 0
      && 39 <= note->key
      && note->key <= 63) {
      if (CVT[note->key - 39] != INVALID) {
        *keyDown |= CVT[note->key - 39];
        *keyUp |= CVT[note->key - 39];
      }
    }
  }
}

int mergeTickTo(Vector_t *v, SkyMusicTick_t *mtr, i16 minInteval) {
  i64 i = -1;
  u64 size;
  SkyMusicTick_t *lastTick;
  vec_size(v, &size);
  if (!size) {
    vec_push(v, mtr);
    return 0;
  }

  vec_at(v, i, (void *)&lastTick);
  if (lastTick->tick > mtr->tick) {
    // Need to insert given tick into the list.
    // Check tick overlapping and inteval between ticks.
    while (lastTick->tick > mtr->tick) {
      // Find the last tick before current tick.
      i--;
      if (!vec_at(v, i, (void *)&lastTick))
        break;
    }
    if (lastTick->tick == mtr->tick) {
      // First check.
      if (
        lastTick->keyDown & mtr->keyDown
        || lastTick->keyUp & mtr->keyUp
        || lastTick->keyDown & mtr->keyUp
        || lastTick->keyUp & mtr->keyDown
      )
        // Error code 1:
        // Tick overlapped.
        return 1;
      // Merge.
      lastTick->keyDown |= mtr->keyDown;
      lastTick->keyUp |= mtr->keyUp;
      return 0;
    } else if (mtr->tick - lastTick->tick < minInteval) {
      // Second check.
      if (
        lastTick->keyDown & mtr->keyDown
        || lastTick->keyUp & mtr->keyUp
        || lastTick->keyDown & mtr->keyUp
        || lastTick->keyUp & mtr->keyDown
      )
        // Error code 2:
        // Tick before is too close or duplicated events.
        return 2;
      else if (i < -1) {
        // Find the first tick after current tick.
        vec_at(v, i + 1, (void *)&lastTick);
        if (lastTick->tick - mtr->tick < minInteval && (
          lastTick->keyDown & mtr->keyDown
          || lastTick->keyUp & mtr->keyUp
          || lastTick->keyDown & mtr->keyUp
          || lastTick->keyUp & mtr->keyDown
        ))
          // Error code 3:
          // Tick after is too close or duplicated events.
          return 3;
      }
    }
    // Insert.
    vec_splice(v, i, 0, mtr, 1);
  } else
    vec_push(v, mtr);

  return 0;
}

/** Convert NBS to key event ticks. */
int buildTicksFrom(SkyAutoPlayOptions_t *options, NBS *nbs, Vector_t *v) {
  f32 tempo = (f32)nbs->header.tempo / 100.
    , tps = options->highTps ? 1000 : 100;
  u32 time = -100
    , th;
  i32 index = 0
    , err;
  u16 keyDown, keyUp;
  size_t s;
  NBSTickEffective *tick;
  SkyMusicTick_t mtr, mti;

  vec_at(&nbs->ticks, 0, (void **)&tick);
  vec_size(&nbs->ticks, &s);
  vec_init(v, sizeof(SkyMusicTick_t));
  th = (int)((float)tick->tick / tempo * tps);
  while (tick) {
    time++;
    // Simulate tick by tick
    if (time < th)
      continue;

    // Build real tick
    buildKeysFrom(tick, &keyDown, &keyUp);
    mtr.tick = th;
    mtr.keyDown = keyDown;
    mtr.keyUp = 0;
    mti.keyDown = 0;
    mti.keyUp = keyUp;

    // Won't do anything when no valid note.
    if (!keyDown && !keyUp)
      goto NextTick;

    // Build inteval tick
    // 20ms inteval
    mti.tick = mtr.tick + 2 * tps / 100;

    // Merge ticks
    err = mergeTickTo(v, &mtr, 2 * tps / 100);
    if (err)
      return err;
    err = mergeTickTo(v, &mti, 2 * tps / 100);
    if (err)
      return err;

NextTick:
    // Next NBS tick
    index++;
    if (index < s) {
      vec_at(&nbs->ticks, index, (void **)&tick);
      th = (int)((float)tick->tick / tempo * tps);
    } else
      tick = NULL;
  }
  // Successfully built
  return 0;
}
