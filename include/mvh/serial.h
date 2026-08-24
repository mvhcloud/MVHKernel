#ifndef MVH_SERIAL_H
#define MVH_SERIAL_H

void serial_init(void);
void serial_put(char value);
int serial_has_data(void);
char serial_read(void);

#endif
