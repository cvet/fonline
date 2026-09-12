// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

int32_t SystemNative_SendFile(intptr_t out_fd, intptr_t in_fd, int64_t offset, int64_t count, int64_t* sent)
{
    assert(sent != NULL);

    int outfd = ToFileDescriptor(out_fd);
    int infd = ToFileDescriptor(in_fd);
    off_t offtOffset = (off_t)offset;
    int savedErrno;

#if HAVE_SENDFILE_4
    ssize_t res;
    while ((res = sendfile(outfd, infd, &offtOffset, (size_t)count)) < 0 && errno == EINTR);
    if (res != -1)
    {
        *sent = res;
        return Error_SUCCESS;
    }

    *sent = 0;
    return SystemNative_ConvertErrorPlatformToPal(errno);

#elif HAVE_SENDFILE_6 || HAVE_SENDFILE_7
    *sent = 0;
    while (1) // in case we need to retry for an EINTR
    {
        off_t len = count;
#if HAVE_SENDFILE_7
        ssize_t res = sendfile(infd, outfd, offtOffset, (size_t)count, NULL, &len, 0);
#else
        ssize_t res = sendfile(infd, outfd, offtOffset, &len, NULL, 0);
#endif
        assert(len >= 0);

        // If the call succeeded, store the number of bytes sent, and return.  We add
        // rather than copy len because a previous call to sendfile could have sent bytes
        // but been interrupted by EINTR, in which case we need to add to that.
        if (res != -1)
        {
            *sent += len;
            return Error_SUCCESS;
        }

        // We got an error. If sendfile "fails" with EINTR or EAGAIN, it may have sent
        // some data that needs to be counted.
        if (errno == EAGAIN || errno == EINTR)
        {
            *sent += len;
            offset += len;
            count -= len;

            // If we actually transferred everything in spite of the error, return success.
            assert(count >= 0);
            if (count == 0) return Error_SUCCESS;

            // For EINTR, loop around and go again.
            if (errno == EINTR) continue;
        }

        // For everything other than EINTR, bail.
        return SystemNative_ConvertErrorPlatformToPal(errno);
    }
#else
    // Emulate sendfile using a simple read/send loop.
    *sent = 0;
    char* buffer = NULL;

    // Save the original input file position and seek to the offset position
    off_t inputFileOrigOffset = lseek(infd, 0, SEEK_CUR);
    if (inputFileOrigOffset == -1 || lseek(infd, offtOffset, SEEK_SET) == -1)
    {
        goto error;
    }

    // Allocate a buffer
    size_t bufferLength = Min((size_t)count, 80 * 1024 * sizeof(char));
    buffer = (char*)malloc(bufferLength);
    if (buffer == NULL)
    {
        goto error;
    }

    // Repeatedly read from the source and write to the destination
    while (count > 0)
    {
        size_t numBytesToRead = Min((size_t)count, bufferLength);

        // Read up to what will fit in our buffer.  We're done if we get back 0 bytes or read 'count' bytes
        ssize_t bytesRead;
        while ((bytesRead = read(infd, buffer, numBytesToRead)) < 0 && errno == EINTR);
        if (bytesRead == -1)
        {
            goto error;
        }
        if (bytesRead == 0)
        {
            break;
        }
        assert(bytesRead > 0);

        // Write what was read.
        ssize_t writeOffset = 0;
        while (bytesRead > 0)
        {
            ssize_t bytesWritten;
            while ((bytesWritten = write(outfd, buffer + writeOffset, (size_t)bytesRead)) < 0 && errno == EINTR);
            if (bytesWritten == -1)
            {
                goto error;
            }
            assert(bytesWritten >= 0);
            bytesRead -= bytesWritten;
            count -= bytesWritten;
            writeOffset += bytesWritten;
            *sent += bytesWritten;
        }
    }

    // Restore the original input file position
    if (lseek(infd, inputFileOrigOffset, SEEK_SET) == -1)
    {
        goto error;
    }

    free(buffer);
    return Error_SUCCESS;

error:
    savedErrno = errno;
    free(buffer);
    return SystemNative_ConvertErrorPlatformToPal(savedErrno);

#endif
}
