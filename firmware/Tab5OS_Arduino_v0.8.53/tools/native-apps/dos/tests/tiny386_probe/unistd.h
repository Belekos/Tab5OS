#ifndef TAB5_RESEARCH_UNISTD_H
#define TAB5_RESEARCH_UNISTD_H
/* No host sleep is needed in the bounded CPU instruction smoke test. */
static inline int usleep(unsigned value) { (void)value; return 0; }
#endif
