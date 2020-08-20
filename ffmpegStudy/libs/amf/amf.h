#ifndef _AMF_H
#define _AMF_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AMF_NUMBER = 0,
    AMF_BOOLEAN,
    AMF_STRING,
    AMF_OBJECT,
    AMF_MOVIECLIP,
    AMF_NULL,
    AMF_UNDEFINED,
    AMF_REFERENCE,
    AMF_ECMA_ARRAY,
    AMF_OBJECT_END,
    AMF_STRICT_ARRAY,
    AMF_DATE,
    AMF_LONG_STRING,
    AMF_UNSUPPORTED,
    AMF_RECORDSET,
    AMF_XML_DOC,
    AMF_TYPED_OBJECT,
    AMF_AVMPLUS,
    AMF_INVALID = 0xff
} AMFDataType;

unsigned int decodeUint16(unsigned char *data)
{
    unsigned int ret = (data[0] << 8) | data[1];
    return ret;
}

unsigned int decodeUint24(unsigned char *data)
{
    unsigned int ret = (data[0] << 16) | (data[1] << 8) | data[2];
    return ret;
}

unsigned int decodeUint32(unsigned char *data)
{
    unsigned int ret = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    return ret;
}

double decodeNumber(unsigned char *data)
{
    double ret;
    unsigned char *ch = (unsigned char *)&ret;
    ch[0] = data[7];
    ch[1] = data[6];
    ch[2] = data[5];
    ch[3] = data[4];
    ch[4] = data[3];
    ch[5] = data[2];
    ch[6] = data[1];
    ch[7] = data[0];

    return ret;
}

unsigned int decodeBool(unsigned char *data)
{
    return (data[0] != 0);
}

#ifdef __cplusplus
}
#endif

#endif
