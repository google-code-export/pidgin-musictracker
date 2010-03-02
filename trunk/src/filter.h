#ifndef __FILTER_H__
#define __FILTER_H__

void filter_profanity(char *str);
void filter_printable(char *str);
const char* filter_get_default(void);

#endif // __FILTER_H__
