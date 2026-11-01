/* Tab5OS ESP32-P4/newlib/lwIP configuration for libssh2 1.11.1. */
#pragma once
#define LIBSSH2_MBEDTLS 1
#define LIBSSH2_LIBRARY 1
#define LIBSSH2_NO_MD5 1
#define LIBSSH2_NO_HMAC_RIPEMD 1
#define LIBSSH2_NO_DSA 1
#define LIBSSH2_NO_RSA_SHA1 1
#define LIBSSH2_NO_AES_CBC 1
#define LIBSSH2_NO_BLOWFISH 1
#define LIBSSH2_NO_RC4 1
#define LIBSSH2_NO_CAST 1
#define LIBSSH2_NO_3DES 1
#define HAVE_UNISTD_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_STRTOLL 1
#define HAVE_SNPRINTF 1
#define HAVE_SELECT 1
#define HAVE_O_NONBLOCK 1
