#ifndef RESPONSE_WRITER_H
#define RESPONSE_WRITER_H

typedef enum
{
    RESPONSE_WRITER_OK = 0U,
    RESPONSE_WRITER_FAIL
} response_writer_status_t;


response_writer_status_t response_writer_start(void);
response_writer_status_t response_writer_stop(void);

#endif