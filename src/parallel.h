/* Splits rows of work between threads. */
#ifndef PARALLEL_H
#define PARALLEL_H

typedef void (*RowFn)(int first, int last);

/* Calls fn(first, last) for slices that cover [0, rows), on several threads (the first slice on the calling one); returns when all are done. The function must only touch what
 * belongs to its rows. Small jobs run on the calling thread. */
void parallel_rows(RowFn fn, int rows);

#endif
