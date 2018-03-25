/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2018 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any
 * later version. Please see the file LICENSE-LGPL for details.
 *
 * Web Page: http://brltty.com/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include <string.h>
#include <errno.h>

#include "log.h"
#include "report.h"
#include "brl_cmds.h"
#include "unicode.h"

#include "scr_driver.h"
#include "system_java.h"

static JNIEnv *env = NULL;
static jclass screenDriverClass = NULL;
static jclass inputHandlersClass = NULL;

static jint screenNumber;
static jint screenColumns;
static jint screenRows;
static jint selectionLeft;
static jint selectionTop;
static jint selectionRight;
static jint selectionBottom;

static const char *problemText;

static int
findScreenDriverClass (void) {
  return findJavaClass(env, &screenDriverClass, JAVA_OBJ_BRLTTY("ScreenDriver"));
}

static int
findInputHandlersClass (void) {
  return findJavaClass(env, &inputHandlersClass, JAVA_OBJ_BRLTTY("InputHandlers"));
}

REPORT_LISTENER(androidScreenDriverReportListener) {
  char *event = parameters->listenerData;

  if (findScreenDriverClass()) {
    static jmethodID method = 0;

    if (findJavaStaticMethod(env, &method, screenDriverClass, "reportEvent",
                             JAVA_SIG_METHOD(JAVA_SIG_VOID,
                                             JAVA_SIG_CHAR // event
                                            ))) {
      (*env)->CallStaticVoidMethod(env, screenDriverClass, method, *event);
      clearJavaException(env, 1);
    }
  }
}

typedef struct {
  ReportListenerInstance *listener;
  ReportIdentifier identifier;
  char character;
} ReportEntry;

static ReportEntry reportEntries[] = {
  { .character = 'b',
    .identifier = REPORT_BRAILLE_DEVICE_ONLINE
  },

  { .character = 'B',
    .identifier = REPORT_BRAILLE_DEVICE_OFFLINE
  },

  { .character = 'k',
    .identifier = REPORT_BRAILLE_KEY_EVENT
  },

  { .character = 0 }
};

static int
construct_AndroidScreen (void) {
  for (ReportEntry *rpt=reportEntries; rpt->character; rpt+=1) {
    rpt->listener = registerReportListener(
      rpt->identifier, androidScreenDriverReportListener, &rpt->character
    );

    if (!rpt->listener) break;
  }

  return 1;
}

static void
destruct_AndroidScreen (void) {
  for (ReportEntry *rpt=reportEntries; rpt->character; rpt+=1) {
    if (rpt->listener) {
      unregisterReportListener(rpt->listener);
      rpt->listener = NULL;
    }
  }
}

static int
poll_AndroidScreen (void) {
  return 0;
}

JAVA_STATIC_METHOD (
  org_a11y_brltty_android_ScreenDriver, screenUpdated, void
) {
  mainScreenUpdated();
}

JAVA_STATIC_METHOD (
  org_a11y_brltty_android_ScreenDriver, exportScreenProperties, void,
  jint number,
  jint columns, jint rows,
  jint left, jint top,
  int right, int bottom
) {
  screenNumber = number;
  screenColumns = columns;
  screenRows = rows;
  selectionLeft = left;
  selectionTop = top;
  selectionRight = right;
  selectionBottom = bottom;
}

static int
refresh_AndroidScreen (void) {
  if (findScreenDriverClass()) {
    static jmethodID method = 0;

    if (findJavaStaticMethod(env, &method, screenDriverClass, "refreshScreen",
                             JAVA_SIG_METHOD(JAVA_SIG_CHAR,
                                            ))) {
      jchar result = (*env)->CallStaticCharMethod(env, screenDriverClass, method);

      if (clearJavaException(env, 1)) {
        problemText = "Java exception";
      } else if (result == 'l') {
        problemText = "Android locked";
      } else if (result == 'r') {
        problemText = "braille released";
      } else {
        problemText = NULL;
      }
    } else {
      problemText = "method not found";
    }
  } else {
    problemText = "class not found";
  }

  return 1;
}

static void
describe_AndroidScreen (ScreenDescription *description) {
  if ((description->unreadable = problemText)) {
    description->cols = strlen(problemText);
    description->rows = 1;
    description->posx = 0;
    description->posy = 0;
    description->number = 0;
  } else {
    description->number = screenNumber;
    description->cols = screenColumns;
    description->rows = screenRows;

    description->cursor = selectionLeft == selectionRight;
    description->posx = selectionLeft;
    description->posy = selectionTop;
  }
}

static int
getRowCharacters (ScreenCharacter *characters, jint rowNumber, jint columnNumber, jint columnCount) {
  if (findScreenDriverClass()) {
    static jmethodID method = 0;

    if (findJavaStaticMethod(env, &method, screenDriverClass, "getRowText",
                             JAVA_SIG_METHOD(JAVA_SIG_VOID, 
                                             JAVA_SIG_ARRAY(JAVA_SIG_CHAR) // textBuffer
                                             JAVA_SIG_INT // rowNumber
                                             JAVA_SIG_INT // columnNumber
                                            ))) {
      jcharArray textBuffer = (*env)->NewCharArray(env, columnCount);

      if (textBuffer) {
        (*env)->CallStaticVoidMethod(env, screenDriverClass, method, textBuffer, rowNumber, columnNumber);

        if (!clearJavaException(env, 1)) {
          jchar buffer[columnCount];
          (*env)->GetCharArrayRegion(env, textBuffer, 0, columnCount, buffer);

          (*env)->DeleteLocalRef(env, textBuffer);
          textBuffer = NULL;

          {
            const jchar *source = buffer;
            const jchar *end = source + columnCount;
            ScreenCharacter *target = characters;

            while (source < end) {
              target->text = *source++;
              target->attributes = SCR_COLOUR_DEFAULT;
              target += 1;
            }
          }

          if ((rowNumber >= selectionTop) && (rowNumber < selectionBottom)) {
            int from = MAX(selectionLeft, columnNumber);
            int to = MIN(selectionRight, columnNumber+columnCount);

            if (from < to) {
              from -= columnNumber;
              to -= columnNumber;

              ScreenCharacter *target = characters + from;
              const ScreenCharacter *end = target + to;

              while (target < end) {
                target->attributes = SCR_COLOUR_FG_BLACK | SCR_COLOUR_BG_LIGHT_GREY;
                target += 1;
              }
            }
          }

          return 1;
        }
      } else {
        logMallocError();
        clearJavaException(env, 0);
      }
    }
  }

  return 0;
}

static int
readCharacters_AndroidScreen (const ScreenBox *box, ScreenCharacter *buffer) {
  if (validateScreenBox(box, screenColumns, screenRows)) {
    if (problemText) {
      setScreenMessage(box, buffer, problemText);
    } else {
      int rowIndex;

      for (rowIndex=0; rowIndex<box->height; rowIndex+=1) {
        if (!getRowCharacters(&buffer[rowIndex * box->width], (rowIndex + box->top), box->left, box->width)) return 0;
      }
    }

    return 1;
  }

  return 0;
}

static int
routeCursor_AndroidScreen (int column, int row, int screen) {
  if (findScreenDriverClass()) {
    static jmethodID method = 0;

    if (findJavaStaticMethod(env, &method, screenDriverClass, "routeCursor",
                             JAVA_SIG_METHOD(JAVA_SIG_BOOLEAN,
                                             JAVA_SIG_INT // column
                                             JAVA_SIG_INT // row
                                            ))) {
      jboolean result = (*env)->CallStaticBooleanMethod(env, screenDriverClass, method, column, row);

      if (!clearJavaException(env, 1)) {
        if (result != JNI_FALSE) {
          return 1;
        }
      }
    }
  }

  return 0;
}

static int
insertKey_AndroidScreen (ScreenKey key) {
  if (findInputHandlersClass()) {
    wchar_t character = key & SCR_KEY_CHAR_MASK;

    setScreenKeyModifiers(&key, 0);

    if (!isSpecialKey(key)) {
      static jmethodID method = 0;

      if (findJavaStaticMethod(env, &method, inputHandlersClass, "inputCharacter",
                               JAVA_SIG_METHOD(JAVA_SIG_BOOLEAN,
                                               JAVA_SIG_CHAR // character
                                              ))) {
        jboolean result = (*env)->CallStaticBooleanMethod(env, inputHandlersClass, method, character);

        if (!clearJavaException(env, 1)) {
          if (result != JNI_FALSE) {
            return 1;
          }
        }
      }
    } else if (character < SCR_KEY_FUNCTION) {
#define SIZE UNICODE_CELL_NUMBER(SCR_KEY_FUNCTION)
#define KEY(key,method) [UNICODE_CELL_NUMBER(SCR_KEY_##key)] = method

      static const char *const methodNames[SIZE] = {
        KEY(ENTER, "inputKey_enter"),
        KEY(TAB, "inputKey_tab"),
        KEY(BACKSPACE, "inputKey_backspace"),
        KEY(ESCAPE, "inputKey_escape"),
        KEY(CURSOR_LEFT, "inputKey_cursorLeft"),
        KEY(CURSOR_RIGHT, "inputKey_cursorRight"),
        KEY(CURSOR_UP, "inputKey_cursorUp"),
        KEY(CURSOR_DOWN, "inputKey_cursorDown"),
        KEY(PAGE_UP, "inputKey_pageUp"),
        KEY(PAGE_DOWN, "inputKey_pageDown"),
        KEY(HOME, "inputKey_home"),
        KEY(END, "inputKey_end"),
        KEY(INSERT, "inputKey_insert"),
        KEY(DELETE, "inputKey_delete"),
      };

      const unsigned int key = UNICODE_CELL_NUMBER(character);
      const char *methodName = methodNames[key];
      if (!methodName) return 0;

      static jmethodID methodIdentifiers[SIZE];
      jmethodID *methodIdentifier = &methodIdentifiers[key];

#undef SIZE
#undef KEY

      if (findJavaStaticMethod(env, methodIdentifier, inputHandlersClass, methodName,
                               JAVA_SIG_METHOD(JAVA_SIG_BOOLEAN,
                                              ))) {
        jboolean result = (*env)->CallStaticBooleanMethod(env, inputHandlersClass, *methodIdentifier);

        if (!clearJavaException(env, 1)) {
          if (result != JNI_FALSE) {
            return 1;
          }
        }
      }
    } else {
      static jmethodID method = 0;

      if (findJavaStaticMethod(env, &method, inputHandlersClass, "inputKey_function",
                               JAVA_SIG_METHOD(JAVA_SIG_BOOLEAN,
                                               JAVA_SIG_INT // key
                                              ))) {
        jboolean result = (*env)->CallStaticBooleanMethod(env, inputHandlersClass, method, character-SCR_KEY_FUNCTION);

        if (!clearJavaException(env, 1)) {
          if (result != JNI_FALSE) {
            return 1;
          }
        }
      }
    }
  }

  logMessage(LOG_WARNING, "unsuported key: %04X", key);
  return 0;
}

static void
scr_initialize (MainScreen *main) {
  initializeRealScreen(main);
  main->base.poll = poll_AndroidScreen;
  main->base.refresh = refresh_AndroidScreen;
  main->base.describe = describe_AndroidScreen;
  main->base.readCharacters = readCharacters_AndroidScreen;
  main->base.routeCursor = routeCursor_AndroidScreen;
  main->base.insertKey = insertKey_AndroidScreen;
  main->construct = construct_AndroidScreen;
  main->destruct = destruct_AndroidScreen;
  env = getJavaNativeInterface();
}
