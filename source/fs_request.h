#pragma once

typedef unsigned char   undefined;

typedef unsigned char    byte;
typedef unsigned int    dword;
typedef long double    longdouble;
typedef long long    longlong;
typedef unsigned char    uchar;
typedef unsigned int    uint;
typedef unsigned long    ulong;
typedef unsigned long long    ulonglong;
typedef unsigned char    undefined1;
typedef unsigned short    undefined2;
typedef unsigned int    undefined4;
typedef unsigned long long    undefined8;
typedef unsigned short    ushort;
typedef unsigned short    wchar16;
typedef unsigned short    word;
typedef struct FAT_WorkMessage FAT_WorkMessage, *PFAT_WorkMessage;

typedef union FAT_Request FAT_Request, *PFAT_Request;

typedef struct FAT_FormatDeviceRequest FAT_FormatDeviceRequest, *PFAT_FormatDeviceRequest;

typedef struct FAT_UnmountRequest FAT_UnmountRequest, *PFAT_UnmountRequest;

typedef struct FAT_MkdirRequest FAT_MkdirRequest, *PFAT_MkdirRequest;

typedef struct FAT_ReadDirRequest FAT_ReadDirRequest, *PFAT_ReadDirRequest;

typedef struct FAT_BaseRequest FAT_BaseRequest, *PFAT_BaseRequest;

typedef uint FSSALHandle, *PFSSALHandle;

typedef int * FAT_DirHandle;

typedef enum SALDeviceType { /* from dimok, unknown accuracy https://github.com/dimok789/mocha/blob/43f1af08a345b333e800ac1e2e18e7aa153e9da9/ios_fs/source/devices.h#L55 */
    SAL_DEVICE_SI=1,
    SAL_DEVICE_ODD=2,
    SAL_DEVICE_SLCCMPT=3,
    SAL_DEVICE_SLC=4,
    SAL_DEVICE_MLC=5,
    SAL_DEVICE_SDCARD=6,
    SAL_DEVICE_SD=7,
    SAL_DEVICE_HFIO=8,
    SAL_DEVICE_RAMDISK=9,
    SAL_DEVICE_USB=17,
    SAL_DEVICE_MLCORIG=18
} __attribute__ ((__packed__)) SALDeviceType;

struct FAT_UnmountRequest {
    int handle;
};

struct FAT_ReadDirRequest {
    FAT_DirHandle handle;
    byte * field1_0x4;
};

struct FAT_BaseRequest {
    char unknown[1028];
};

struct FAT_MkdirRequest {
    char path[512];
    uint permissions;
};

// struct FSSALHandle {
//     enum SALDeviceType type;
//     byte index;
//     ushort generation;
// };

struct FAT_FormatDeviceRequest {
    FSSALHandle device_handle;
};

struct FAT_OpenFileRequest{
    char *mode;
    char path[512];
    void **filehandle_out_ptr; //PF_FILE
    uint unknown1;
    uint unknown2;
} typedef FAT_OpenFileRequest;

union FAT_Request {
    struct FAT_FormatDeviceRequest format_device;
    struct FAT_UnmountRequest unmount;
    struct FAT_MkdirRequest mkdir;
    struct FAT_ReadDirRequest readdir;
    struct FAT_BaseRequest _b; /* fake to pad struct */
    FAT_OpenFileRequest open_file;
};


struct fs_response {
    int set;
    int retval;
} typedef fs_response;

struct FAT_WorkMessage {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    undefined field4_0x4;
    undefined field5_0x5;
    undefined field6_0x6;
    undefined field7_0x7;
    uint flags;
    undefined field9_0xc;
    undefined field10_0xd;
    undefined field11_0xe;
    undefined field12_0xf;
    FSSALHandle handle;
    fs_response * worker;
    uint command;
    void (*callback)(int, void*);
    void* calback_data;
    union FAT_Request request;
};

