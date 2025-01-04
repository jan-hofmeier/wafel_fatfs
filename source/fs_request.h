#pragma once

#include "wut_structsize.h"
#include <wafel/types.h>


/* WUT Stuff */

typedef uint32_t FSDirectoryHandle;
typedef uint32_t FSFileHandle;
typedef uint32_t FSPriority;
typedef uint64_t FSTime;

typedef enum FSError
{
   FS_ERROR_OK                      = 0,
   FS_ERROR_NOT_INIT                = -0x30001,
   FS_ERROR_BUSY                    = -0x30002,
   FS_ERROR_CANCELLED               = -0x30003,
   FS_ERROR_END_OF_DIR              = -0x30004,
   FS_ERROR_END_OF_FILE             = -0x30005,
   FS_ERROR_MAX_MOUNT_POINTS        = -0x30010,
   FS_ERROR_MAX_VOLUMES             = -0x30011,
   FS_ERROR_MAX_CLIENTS             = -0x30012,
   FS_ERROR_MAX_FILES               = -0x30013,
   FS_ERROR_MAX_DIRS                = -0x30014,
   FS_ERROR_ALREADY_OPEN            = -0x30015,
   FS_ERROR_ALREADY_EXISTS          = -0x30016,
   FS_ERROR_NOT_FOUND               = -0x30017,
   FS_ERROR_NOT_EMPTY               = -0x30018,
   FS_ERROR_ACCESS_ERROR            = -0x30019,
   FS_ERROR_PERMISSION_ERROR        = -0x3001A,
   FS_ERROR_DATA_CORRUPTED          = -0x3001B,
   FS_ERROR_STORAGE_FULL            = -0x3001C,
   FS_ERROR_JOURNAL_FULL            = -0x3001D,
   FS_ERROR_UNAVAILABLE_COMMAND     = -0x3001F,
   FS_ERROR_UNSUPPORTED_COMMAND     = -0x30020,
   FS_ERROR_INVALID_PARAM           = -0x30021,
   FS_ERROR_INVALID_PATH            = -0x30022,
   FS_ERROR_INVALID_BUFFER          = -0x30023,
   FS_ERROR_INVALID_ALIGNMENT       = -0x30024,
   FS_ERROR_INVALID_CLIENTHANDLE    = -0x30025,
   FS_ERROR_INVALID_FILEHANDLE      = -0x30026,
   FS_ERROR_INVALID_DIRHANDLE       = -0x30027,
   FS_ERROR_NOT_FILE                = -0x30028,
   FS_ERROR_NOT_DIR                 = -0x30029,
   FS_ERROR_FILE_TOO_BIG            = -0x3002A,
   FS_ERROR_OUT_OF_RANGE            = -0x3002B,
   FS_ERROR_OUT_OF_RESOURCES        = -0x3002C,
   FS_ERROR_MEDIA_NOT_READY         = -0x30040,
   FS_ERROR_MEDIA_ERROR             = -0x30041,
   FS_ERROR_WRITE_PROTECTED         = -0x30042,
   FS_ERROR_INVALID_MEDIA           = -0x30043,
} FSError;

typedef enum FSErrorFlag
{
   FS_ERROR_FLAG_NONE               =  0x0,
   FS_ERROR_FLAG_MAX                =  0x1,
   FS_ERROR_FLAG_ALREADY_OPEN       =  0x2,
   FS_ERROR_FLAG_EXISTS             =  0x4,
   FS_ERROR_FLAG_NOT_FOUND          =  0x8,
   FS_ERROR_FLAG_NOT_FILE           =  0x10,
   FS_ERROR_FLAG_NOT_DIR            =  0x20,
   FS_ERROR_FLAG_ACCESS_ERROR       =  0x40,
   FS_ERROR_FLAG_PERMISSION_ERROR   =  0x80,
   FS_ERROR_FLAG_FILE_TOO_BIG       =  0x100,
   FS_ERROR_FLAG_STORAGE_FULL       =  0x200,
   FS_ERROR_FLAG_UNSUPPORTED_CMD    =  0x400,
   FS_ERROR_FLAG_JOURNAL_FULL       =  0x800,
   FS_ERROR_FLAG_ALL                =  0xFFFFFFFF,
} FSErrorFlag;

typedef enum FSStatus
{
   FS_STATUS_OK                     = 0,
   FS_STATUS_CANCELLED              = -1,
   FS_STATUS_END                    = -2,
   FS_STATUS_MAX                    = -3,
   FS_STATUS_ALREADY_OPEN           = -4,
   FS_STATUS_EXISTS                 = -5,
   FS_STATUS_NOT_FOUND              = -6,
   FS_STATUS_NOT_FILE               = -7,
   FS_STATUS_NOT_DIR                = -8,
   FS_STATUS_ACCESS_ERROR           = -9,
   FS_STATUS_PERMISSION_ERROR       = -10,
   FS_STATUS_FILE_TOO_BIG           = -11,
   FS_STATUS_STORAGE_FULL           = -12,
   FS_STATUS_JOURNAL_FULL           = -13,
   FS_STATUS_UNSUPPORTED_CMD        = -14,
   FS_STATUS_MEDIA_NOT_READY        = -15,
   FS_STATUS_MEDIA_ERROR            = -17,
   FS_STATUS_CORRUPTED              = -18,
   FS_STATUS_FATAL_ERROR            = -0x400,
} FSStatus;

typedef enum FSMode :u32
{
   FS_MODE_READ_OWNER                   = 0x400,
   FS_MODE_WRITE_OWNER                  = 0x200,
   FS_MODE_EXEC_OWNER                   = 0x100,

   FS_MODE_READ_GROUP                   = 0x040,
   FS_MODE_WRITE_GROUP                  = 0x020,
   FS_MODE_EXEC_GROUP                   = 0x010,

   FS_MODE_READ_OTHER                   = 0x004,
   FS_MODE_WRITE_OTHER                  = 0x002,
   FS_MODE_EXEC_OTHER                   = 0x001,
} FSMode;

//! Flags for \link FSStat \endlink.
//! One can return multiple flags, so for example a file that's encrypted or a linked directory.
typedef enum FSStatFlags : u32
{
   //! The retrieved file entry is a (link to a) directory.
   FS_STAT_DIRECTORY                = 0x80000000,
   //! The retrieved file entry also has a quota set.
   FS_STAT_QUOTA                    = 0x60000000,
   //! The retrieved file entry is a (link to a) file.
   FS_STAT_FILE                     = 0x01000000,
   //! The retrieved file entry also is encrypted and can't be opened (see vWii files for example).
   FS_STAT_ENCRYPTED_FILE           = 0x00800000,
   //! The retrieved file entry also is a link to a different file on the filesystem.
   //! Note: It's currently not known how one can read the linked-to file entry.
   FS_STAT_LINK                     = 0x00010000,
} FSStatFlags;

typedef enum FSVolumeState
{
   FS_VOLUME_STATE_INITIAL          = 0,
   FS_VOLUME_STATE_READY            = 1,
   FS_VOLUME_STATE_NO_MEDIA         = 2,
   FS_VOLUME_STATE_INVALID_MEDIA    = 3,
   FS_VOLUME_STATE_DIRTY_MEDIA      = 4,
   FS_VOLUME_STATE_WRONG_MEDIA      = 5,
   FS_VOLUME_STATE_MEDIA_ERROR      = 6,
   FS_VOLUME_STATE_DATA_CORRUPTED   = 7,
   FS_VOLUME_STATE_WRITE_PROTECTED  = 8,
   FS_VOLUME_STATE_JOURNAL_FULL     = 9,
   FS_VOLUME_STATE_FATAL            = 10,
   FS_VOLUME_STATE_INVALID          = 11,
} FSVolumeState;

typedef enum FSMediaState {
    FS_MEDIA_STATE_READY = 0,
    FS_MEDIA_STATE_NO_MEDIA = 1,
    FS_MEDIA_STATE_INVALID_MEDIA = 2,
    FS_MEDIA_STATE_DIRTY_MEDIA = 3,
    FS_MEDIA_STATE_MEDIA_ERROR = 4,
} FSMediaState;


typedef struct WUT_PACKED FSStat
{
   FSStatFlags flags;
   FSMode mode;
   uint32_t owner;
   uint32_t group;
   uint32_t size;
   uint32_t allocSize;
   uint64_t quotaSize;
   uint32_t entryId;
   FSTime created;
   FSTime modified;
   WUT_UNKNOWN_BYTES(0x30);
} FSStat;

WUT_CHECK_OFFSET(FSStat, 0x00, flags);
WUT_CHECK_OFFSET(FSStat, 0x04, mode);
WUT_CHECK_OFFSET(FSStat, 0x08, owner);
WUT_CHECK_OFFSET(FSStat, 0x0C, group);
WUT_CHECK_OFFSET(FSStat, 0x10, size);
WUT_CHECK_OFFSET(FSStat, 0x14, allocSize);
WUT_CHECK_OFFSET(FSStat, 0x18, quotaSize);
WUT_CHECK_OFFSET(FSStat, 0x20, entryId);
WUT_CHECK_OFFSET(FSStat, 0x24, created);
WUT_CHECK_OFFSET(FSStat, 0x2C, modified);
WUT_CHECK_SIZE(FSStat, 0x64);

typedef struct FSDirectoryEntry
{
   FSStat info;
   char name[256];
} FSDirectoryEntry;
WUT_CHECK_OFFSET(FSDirectoryEntry, 0x64, name);
WUT_CHECK_SIZE(FSDirectoryEntry, 0x164);

/* WUT Stuff  end*/


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

typedef struct FAT_UnmountRequest {
    int handle;
} FAT_UnmountRequest;

typedef struct FAT_ReadDirRequest {
    void **dp;
    FSDirectoryEntry *entry;
} FAT_ReadDirRequest;

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

typedef struct FAT_StatFileRequest {
    void **fp;
    FSStat *stat;
} FAT_StatFileRequest;

typedef enum FSReadRequestFlag : uint32_t {
    READ_REQUEST_WITH_POS = 0x1,
} FSReadRequestFlag;

typedef struct FAT_ReadFileRequest {
    void * buffer;
    size_t size;
    size_t count;
    uint pos;
    void **file;
    FSReadRequestFlag flags; /* 0x1 means WithPos */
} FAT_ReadFileRequest;

typedef struct FAT_SetPosFileRequest {
    void **file;
    uint32_t pos;
} FAT_SetPosFileRequest;

typedef struct FAT_CloseFileRequest {
    void **file;
} FAT_CloseFileRequest;

typedef struct FAT_OpenDirRequest {
    char path[512];
    void **dirhandle_out_ptr; /* pointer needs prefill? */
} FAT_OpenDirRequest;

typedef struct FAT_CloseDirRequst {
    void **dp;
} FAT_CloseDirRequst;

typedef enum FS_StatFSType : uint32_t {
    FS_STAT_FREE_SPACE = 0,
    FS_STAT_DIR_SIZE = 1,
    FS_STAT_ENTRY_NUM = 2,
    FS_STAT_FSINFO = 3,
    FS_STAT_UKN_4 = 4,
    FS_STAT_INIT_STAT = 5,
    FS_STAT_UKN_7 = 7
} FS_StatFSType;

typedef struct FAT_StatFSRequest {
    char path[512];
    FS_StatFSType type;
    void * out_ptr;
} FAT_StatFSRequest;

union FAT_Request {
    struct FAT_FormatDeviceRequest format_device;
    FAT_UnmountRequest unmount;
    struct FAT_MkdirRequest mkdir;
    struct FAT_BaseRequest _b; /* fake to pad struct */
    FAT_OpenDirRequest open_dir;
    FAT_ReadDirRequest read_dir;
    FAT_CloseDirRequst close_dir;
    FAT_OpenFileRequest open_file;
    FAT_ReadFileRequest read_file;
    FAT_SetPosFileRequest setpos_file;
    FAT_StatFileRequest stat_file;
    FAT_CloseFileRequest close_file;
    FAT_StatFSRequest stat_fs;
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
    uint volume_handle;
    fs_response * worker;
    uint command;
    void (*callback)(FSError, void*);
    void* calback_data;
    union FAT_Request request;
};

typedef struct FSVolumeArgs {
    uint * driver_ctx; /* handles, device_params, arb pointers.. */
    int field1_0x4;
    int state;
    uint vfs_devtype;
    uint field4_0x10;
    uint device_handle;
    uint start_lba_hi;
    uint start_lba;
    uint block_count;
    uint block_count_hi;
    uint block_size;
    uint partition_count;
    uint partition_no;
    uint field13_0x34;
    short field14_0x38;
    short field15_0x3a;
    short field16_0x3c;
    short field17_0x3e;
    short field18_0x40;
    undefined field19_0x42;
    undefined field20_0x43;
    uint field21_0x44;
    uint field22_0x48;
    uint allowed_ops_ex;
    short device_type_hi;
    short device_type;
    uint alignment_smth;
    uint field27_0x58;
    char field28_0x5c[128];
    char field29_0xdc[128];
    int (* cb)(struct FAT_WorkMessage *);
} FSVolumeArgs;

typedef struct FSLinkedQueueEntry {
    struct FSLinkedQueueEntry * next;
} FSLinkedQueueEntry;

typedef struct FSSALDevice {
    FSLinkedQueueEntry link;
    FSSALHandle handle;
    struct FSSALFilesystem * filesystem;
    uint server_handle;
    uint device_type;
    uint field5_0x14;
    uint allowed_ops;
    uint media_state;
    uint max_lba_size_hi;
    uint max_lba_size;
    uint field10_0x28;
    uint field11_0x2c;
    uint block_count;
    uint block_count_hi;
    uint block_size;
    uint field15_0x3c;
    uint alignment_smth;
    uint field17_0x44;
    uint field18_0x48;
    uint field19_0x4c;
    uint field20_0x50;
    char field21_0x54[128];
    char field22_0xd4[128];
    char field23_0x154[128];
    uint32_t (* op_read)(void *, uint32_t, uint32_t, uint32_t, uint32_t, void *, uint32_t, void *);
    uint32_t (* op_read_2)(void *, uint32_t, uint32_t, uint32_t, uint32_t, void *, uint32_t, void *); /* Created by retype action */
    uint op_write;
    uint op_write_2;
    uint field28_0x1e4;
    uint field29_0x1e8;
    int (* op_sync)(int, uint32_t, uint32_t, uint32_t, void *, void *);
    uint field31_0x1f0;
    uint field32_0x1f4;
    uint field33_0x1f8;
    uint field34_0x1fc;
    uint field35_0x200;
} FSSALDevice;
