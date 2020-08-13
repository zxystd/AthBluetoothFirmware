//
//  LinuxTypes.h
//  Ath3kBT
//
//  Created by qcwap on 2020/8/12.
//  Copyright © 2020 zxystd. All rights reserved.
//

#ifndef LinuxTypes_h
#define LinuxTypes_h

typedef UInt8  u8;
typedef UInt16 u16;
typedef UInt32 u32;
typedef UInt64 u64;

typedef u8 __u8;
typedef u16 __u16;
typedef u32 __u32;
typedef u64 __u64;

typedef  SInt16 __be16;
typedef  SInt32 __be32;
typedef  SInt64 __be64;
typedef  SInt16 __le16;
typedef  SInt32 __le32;
typedef  SInt64 __le64;

typedef SInt8  s8;
typedef SInt16 s16;
typedef SInt32 s32;
typedef SInt64 s64;

typedef s8  __s8;
typedef s16 __s16;
typedef s32 __s32;
typedef s64 __s64;

#define __force
#define __le32_to_cpu(x) ((__force __u32)(__le32)(x))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static inline __u32 __le32_to_cpup(const __le32 *p)
{
    return (__force __u32)*p;
}

static inline u32 get_unaligned_le32(const void *p)
{
    return __le32_to_cpup((__le32 *)p);
}

#endif /* LinuxTypes_h */
