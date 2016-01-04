//
//  kfslib.c
//  KFS
//
//  Copyright (c) 2012, FadingRed LLC
//  All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
//  following conditions are met:
//  
//    - Redistributions of source code must retain the above copyright notice, this list of conditions and the
//      following disclaimer.
//    - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
//    - Neither the name of the FadingRed LLC nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mount.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <sys/errno.h>

#include "kfslib.h"
#include "internal.h"
#include "mountargs.h"
#include "backends/nfs/nfs3programs.h"

int _rpcpmstart;
int _rpcsvcdirty;

const char *kfs_devprefix = "duck";

extern int svc_maxfd;

#define _msgout(format, ...) do { fprintf(stderr, format "\n", ##__VA_ARGS__); } while (0)
#define _errout(format, ...) do { fprintf(stderr, format " %i: %s\n", ##__VA_ARGS__, errno, strerror(errno)); } while (0)

int kfs_mount(kfsfilesystem_t *filesystem, in_port_t nfs_port) {
    // get a unique identifier
    kfsid_t identifier = kfstable_put(filesystem);

    // setup arguments
    char *fshandle = NULL;
    asprintf(&fshandle, "%llu", identifier);

    char *hostname = NULL;
    asprintf(&hostname, "%s-%llu", kfs_devprefix, identifier);

    struct sockaddr_in nfsaddr = (struct sockaddr_in) {
            .sin_family = AF_INET,
            .sin_port = nfs_port,
            .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    struct nfs_args3 args = (struct nfs_args3) {
            .version = NFS_V3,
            .addr = (struct sockaddr *) &nfsaddr,
            .addrlen = sizeof(struct sockaddr_in),
            .sotype = SOCK_STREAM,
            .proto = IPPROTO_TCP,
            .fh = (u_char *) fshandle,
            .fhsize = (int) strlen(fshandle),
            .flags = NFSMNT_NFSV3 | NFSMNT_INT | NFSMNT_WSIZE | NFSMNT_RSIZE | NFSMNT_READDIRSIZE |
                     NFSMNT_TIMEO | NFSMNT_NOLOCKS | NFSMNT_NOQUOTA,
//            The maximum number of bytes per network WRITE request that the NFS client can send when writing data to a file on an NFS server. The actual data payload size of each NFS WRITE request is equal to or smaller than the wsize setting. The largest write payload supported by the Linux NFS client is 1,048,576 bytes (one megabyte).
//            Similar to rsize , the wsize value is a positive integral multiple of 1024. Specified wsize values lower than 1024 are replaced with 4096; values larger than 1048576 are replaced with 1048576. If a specified value is within the supported range but not a multiple of 1024, it is rounded down to the nearest multiple of 1024.
//            If a wsize value is not specified, or if the specified wsize value is larger than the maximum that either client or server can support, the client and server negotiate the largest wsize value that they can both support.
            .wsize = WRITE_MAX_LEN,
//            The maximum number of bytes in each network READ request that the NFS client can receive when reading data from a file on an NFS server. The actual data payload size of each NFS READ request is equal to or smaller than the rsize setting. The largest read payload supported by the Linux NFS client is 1,048,576 bytes (one megabyte).
//            The rsize value is a positive integral multiple of 1024. Specified rsize values lower than 1024 are replaced with 4096; values larger than 1048576 are replaced with 1048576. If a specified value is within the supported range but not a multiple of 1024, it is rounded down to the nearest multiple of 1024.
//            If an rsize value is not specified, or if the specified rsize value is larger than the maximum that either client or server can support, the client and server negotiate the largest rsize value that they can both support.
            .rsize = READ_MAX_LEN,
            .readdirsize = DIR_MAX_LEN,
            // initial timeout in .1 secs
//            The time in deciseconds (tenths of a second) the NFS client waits for a response before it retries an NFS request.
//            For NFS over TCP the default timeo value is 600 (60 seconds). The NFS client performs linear backoff: After each retransmission the timeout is increased by timeo up to the maximum of 600 seconds.
//            However, for NFS over UDP, the client uses an adaptive algorithm to estimate an appropriate timeout value for frequently used request types (such as READ and WRITE requests), but uses the timeo setting for infrequently used request types (such as FSINFO requests). If the timeo option is not specified, infrequently used request types are retried after 1.1 seconds. After each retransmission, the NFS client doubles the timeout for that request, up to a maximum timeout length of 60 seconds.
            .timeo = 600,
//            The number of times the NFS client retries a request before it attempts further recovery action. If the retrans option is not specified, the NFS client tries each request three times.
//            The NFS client generates a "server not responding" message after retrans retries, then attempts further recovery (depending on whether the hard mount option is in effect).
            .retrans = 0,
            .maxgrouplist = 0,
            .readahead = 0,
            .hostname = hostname,
    };
    int flags = MNT_NOATIME /* disable update of file access time */
                | MNT_NOUSERXATTR /* Don't allow user extended attributes */
                | MNT_IGNORE_OWNERSHIP /* VFS will ignore ownership information on filesystem objects */
                | MNT_NOEXEC
                | MNT_LOCAL
                ;
    int error = mount("nfs", filesystem->options.mountpoint, flags, &args);
    if (error == -1) {
        // returns the value 0 if the mount was successful, otherwise -1 is returned and the
        // variable errno is set to indicate the error
        return errno;
    }
    return error;
}

int kfs_unmount(kfsfilesystem_t *filesystem) {
    // unmount the filesystem
    int error = unmount(filesystem->options.mountpoint, MNT_FORCE);
    if (error == -1) {
        // Umount returns the value 0 if the umount succeeded; otherwise -1 is returned and the
        // variable errno is set to indicate the error.
        return errno;
    }
	// remove the entry from our table
	kfstable_remove(filesystem->identifier());
    return error;
}

#pragma mark -
#pragma mark running the nfs server

// ----------------------------------------------------------------------------------------------------
// running the nfs server
// ----------------------------------------------------------------------------------------------------
int kfs_register_nfs_server() {
    // create and bind a new socket for kfs to use
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(0),
            .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        _errout("bind failed.");
        return -1;
    }

    // get the socket name and store off the port we're using
    struct sockaddr_in baddr = {};
    socklen_t len = sizeof(baddr);
    if (getsockname(sock, (struct sockaddr *) &baddr, &len) != 0) {
        _errout("getsockname failed.");
        return -1;
    }
    in_port_t nfs_port = baddr.sin_port;

    // create the service, then register the nfs and mount programs. we supply
    // a protocol of 0 here so that the programs aren't registered with portmap
    // (as per the documentation).
    SVCXPRT *transp = svctcp_create(sock, 0, 0);
    if (transp == NULL) {
        _msgout("cannot create tcp service.");
        return -1;
    }
    if (!svc_register(transp, NFS_PROGRAM, NFS_V3, nfs_program_3, 0)) {
        _msgout("unable to register (NFS_PROGRAM, NFS_V3, tcp).");
        return -1;
    }
    if (!svc_register(transp, MOUNT_PROGRAM, MOUNT_V3, mount_program_3, 0)) {
        _msgout("unable to register (MOUNT_PROGRAM, MOUNT_V3, tcp).");
        return -1;
    }
    return nfs_port;
}

void kfs_run_nfs_server(kfs_signal* status(void)) {
	// svc_run();
    fd_set readfds;
    while (status()) {
        readfds = svc_fdset;
        switch (select(svc_maxfd + 1, &readfds, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0)) {
            case -1:
                if (errno == EINTR) {
                    // Interrupted system call
                    continue;
                }
                perror("svc_run: - select failed");
                return;
            case 0:
                continue;
            default:
                svc_getreqset(&readfds);
        }
    }
    // Unregister the nfs and mount programs.
    svc_unregister(NFS_PROGRAM, NFS_V3);
    svc_unregister(MOUNT_PROGRAM, MOUNT_V3);
}

#pragma mark -
#pragma mark type implementations
// ----------------------------------------------------------------------------------------------------
// type implementations
// ----------------------------------------------------------------------------------------------------

kfscontents_t *kfscontents_create(void) {
    return calloc(1, sizeof(struct kfscontents));
}

void kfscontents_destroy(kfscontents_t *contents) {
    if (contents) {
        for (uint64_t i = 0; i < kfscontents_count(contents); i++) {
            free((void *) kfscontents_at(contents, i));
        }
        free(contents);
    }
}

void kfscontents_append(kfscontents_t *contents, const char *entry) {
    uint64_t cap = contents->capacity;
    uint64_t pos = contents->count;
    uint64_t len = contents->count + 1;

    if (cap < len) {
        if (cap == 0) { cap = 1; }
        else { cap *= 2; }

        contents->entries = realloc(contents->entries, sizeof(const char *) * cap);
    }

    contents->entries[pos] = strdup(entry);
    contents->capacity = cap;
    contents->count = len;
}

uint64_t kfscontents_count(kfscontents_t *contents) {
    return contents->count;
}

const char *kfscontents_at(kfscontents_t *contents, uint64_t idx) {
    const char *entry = NULL;
    if (idx < contents->count) {
        entry = contents->entries[idx];
    }
    return entry;
}
