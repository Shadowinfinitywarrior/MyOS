#ifndef STDARG_H
#define STDARG_H

typedef char *va_list;

#define va_start(ap, last) ((ap) = (char *)(&last) + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap) += sizeof(type) - 1, (ap) - sizeof(type)))
#define va_end(ap) ((void)0)

#endif
