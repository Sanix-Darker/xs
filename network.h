#ifndef NETWORK_H
#define NETWORK_H

void network_init(void);
void network_cleanup(void);
char* fetch_url(const char* url);

#endif
