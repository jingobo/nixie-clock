#ifndef __HTTPD_H
#define __HTTPD_H

#include <ipc.h>

// ��������� �������� ������ ��� Web
extern ipc_processor_proxy_t httpd_processor_in;

// ������������� ��� �������
void httpd_init(void);

#endif // __HTTPD_H
