// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#if (HAVE_PREADV || HAVE_PWRITEV) && !defined(TARGET_WASM)
static int GetAllowedVectorCount(IOVector* vectors, int32_t vectorCount)
{
#if defined(IOV_MAX)
    const int IovMax = IOV_MAX;
#else
    // In theory all the platforms that we support define IOV_MAX,
    // but we want to be extra safe and provde a fallback
    // in case it turns out to not be true.
    // 16 is low, but supported on every platform.
    const int IovMax = 16;
#endif

    int allowedCount = (int)vectorCount;

    // We need to respect the limit of items that can be passed in iov.
    // In case of writes, the managed code is responsible for handling incomplete writes.
    // In case of reads, we simply returns the number of bytes read and it's up to the users.
    if (IovMax < allowedCount)
    {
        allowedCount = IovMax;
    }

#if defined(TARGET_APPLE)
    // For macOS preadv and pwritev can fail with EINVAL when the total length
    // of all vectors overflows a 32-bit integer.
    size_t totalLength = 0;
    for (int i = 0; i < allowedCount; i++)
    {
        assert(INT_MAX >= vectors[i].Count);

        totalLength += vectors[i].Count;

        if (totalLength > INT_MAX)
        {
            allowedCount = i;
            break;
        }
    }
#else
    (void)vectors;
#endif

    return allowedCount;
}
#endif // (HAVE_PREADV || HAVE_PWRITEV) && !defined(TARGET_WASM)

int64_t SystemNative_PReadV(intptr_t fd, IOVector* vectors, int32_t vectorCount, int64_t fileOffset)
{
    assert(vectors != NULL);
    assert(vectorCount >= 0);

    int64_t count = 0;
    int fileDescriptor = ToFileDescriptor(fd);
#if HAVE_PREADV && !defined(TARGET_WASM) // preadv is buggy on WASM
    int allowedVectorCount = GetAllowedVectorCount(vectors, vectorCount);
    while ((count = preadv(fileDescriptor, (struct iovec*)vectors, allowedVectorCount, (off_t)fileOffset)) < 0 && errno == EINTR);
#else
    int64_t current;
    for (int i = 0; i < vectorCount; i++)
    {
        IOVector vector = vectors[i];
        while ((current = pread(fileDescriptor, vector.Base, vector.Count, (off_t)(fileOffset + count))) < 0 && errno == EINTR);

        if (current < 0)
        {
            // if previous calls were successful, we return what we got so far
            // otherwise, we return the error code
            return count > 0 ? count : current;
        }

        count += current;

        // Incomplete pread operation may happen for two reasons:
        // a) We have reached EOF.
        // b) The operation was interrupted by a signal handler.
        // To mimic preadv, we stop on the first incomplete operation.
        if (current != (int64_t)vector.Count)
        {
            return count;
        }
    }
#endif

    assert(count >= -1);
    return count;
}

int64_t SystemNative_PWriteV(intptr_t fd, IOVector* vectors, int32_t vectorCount, int64_t fileOffset)
{
    assert(vectors != NULL);
    assert(vectorCount >= 0);

    int64_t count = 0;
    int fileDescriptor = ToFileDescriptor(fd);
#if HAVE_PWRITEV && !defined(TARGET_WASM) // pwritev is buggy on WASM
    int allowedVectorCount = GetAllowedVectorCount(vectors, vectorCount);
    while ((count = pwritev(fileDescriptor, (struct iovec*)vectors, allowedVectorCount, (off_t)fileOffset)) < 0 && errno == EINTR);
#else
    int64_t current;
    for (int i = 0; i < vectorCount; i++)
    {
        IOVector vector = vectors[i];
        while ((current = pwrite(fileDescriptor, vector.Base, vector.Count, (off_t)(fileOffset + count))) < 0 && errno == EINTR);

        if (current < 0)
        {
            // if previous calls were successful, we return what we got so far
            // otherwise, we return the error code
            return count > 0 ? count : current;
        }

        count += current;

        // Incomplete pwrite operation may happen for few reasons:
        // a) There was not enough space available or the file is too large for given file system.
        // b) The operation was interrupted by a signal handler.
        // To mimic pwritev, we stop on the first incomplete operation.
        if (current != (int64_t)vector.Count)
        {
            return count;
        }
    }
#endif

    assert(count >= -1);
    return count;
}
