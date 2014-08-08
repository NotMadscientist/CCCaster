// File: ConsoleDefintions.h
// Title: Definitions for dimensions, menu results, and key presses
// Author: Jeff Benson
// Date: 8/2/11

#ifndef _PRAGMA_ONCE_CONSOLEDEFS_H_
#define _PRAGMA_ONCE_CONSOLEDEFS_H_
extern int MAXSCREENX;
extern int MAXSCREENY;
#define BADMENU             (60000)
#define USERESC             (60001)
#define USERDELETE          (60002)
#define UP_KEY              (72)
#define DOWN_KEY            (80)
#define LEFT_KEY            (75)
#define RIGHT_KEY           (77)
#define HOME_KEY            (71)
#define END_KEY             (79)
#define BACKSPACE_KEY       (8)
#define CONTROL_C_KEY       (3)
#define CONTROL_V_KEY       (22)
#define DELETE_KEY          (83)
#define RETURN_KEY          (13)
#define ESCAPE_KEY          (27)
#define KB_EXTENDED_KEY     (224)                       // Returned from kbhit if an extended key is pressed
#endif