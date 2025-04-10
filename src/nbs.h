#ifndef _INC_NBS_
#define _INC_NBS_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *buffer;
  size_t cursor;
} NBSReader;

typedef struct {
  short songLengthOld;
  char version;
  char instCtr;
  short songLength;
  short layerCtr;
  char *name;
  char *author;
  char *originAuthor;
  char *desc;
  short tempo;
  char autoSave;
  char autoSaveDur;
  char timeSign;
  int minutes;
  int lc;
  int rc;
  int noteAdded;
  int noteRemoved;
  char *midiName;
  char loop;
  char maxLoopCtr;
  short loopStartTick;
} NBSHeader;

typedef struct {
  int tick;
  int layer;
  char instrument;
  char key;
  char velocity;
  unsigned char panning;
  short pitch;
} NBSNoteBlock;

typedef struct __NBSTickEffective {
  int tick;
  int ticksToNext;
  int noteCtr;
  struct __NBSTickEffective *next;
  struct __NBSTickEffective *prev;
  NBSNoteBlock *notes;
} NBSTickEffective;

typedef struct {
  NBSHeader header;
  int effectiveTick;
  NBSTickEffective *ticks;
} NBS;

size_t readNBSFile(const char *buffer, size_t cursor, NBS *nbs);
void freeNBSFile(NBS *nbs);

#endif