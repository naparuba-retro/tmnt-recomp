/* Diagnostic helpers for the command-line tools (not used by the game logic). */
#ifndef DIAG_H
#define DIAG_H

/* Starts a thread that, after `seconds`, prints the instruction pointer and likely return addresses of the main thread and ends the
 * process with code 3 : finds out where a run hangs (look the address up in tmnt_replay.map). No-op outside Windows. */
void diag_start_watchdog(int seconds);

#endif
