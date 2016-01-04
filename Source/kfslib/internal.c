//
//  table.c
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

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

#include "internal.h"

#define MAX_FILESYSTEMS 100

#pragma mark -
#pragma mark default implementation
// ----------------------------------------------------------------------------------------------------
// default implementation
// ----------------------------------------------------------------------------------------------------

//#define KFS_DEBUG_LOG
#ifdef KFS_DEBUG_LOG
#define klog(format, ...) do { fprintf(stderr, format "\n", ##__VA_ARGS__); } while (0)
#else
#define klog(format, ...)
#endif

static const kfsfilesystem_t *table[MAX_FILESYSTEMS];

const kfsfilesystem_t *get_filesystem_from_handle(const char *handle, const char **outPath, uint64_t *outIdentifier) {
	klog("%s (handle) get_filesystem_from_handle)", handle);
	if(!handle) {
		return NULL;
	}
	// Make sure to modify copy of file handle
	char *fileid_str = strdup(handle);
	char *fsid_str = strsep(&fileid_str, ":");
	const kfsfilesystem_t *filesystem = kfstable_get(atoll(fsid_str));
	if(!filesystem) {
		return NULL;
	}
	if (outPath) {
    	uint32_t fileid = fileid_str ? atoi(fileid_str) : -1;
        klog("Get path from file id %u (fileid) with fileidreverse", fileid);
		*outPath = filesystem->fileidreverse(fileid);
	}
	if (outIdentifier) {
		*outIdentifier = atoll(fsid_str);
	}
	return filesystem;
}

kfsid_t kfstable_put(const kfsfilesystem_t *filesystem) {
	kfsid_t identifier = filesystem->identifier();
	klog("kfstable_put %lli (kfsid_t) for filesystem %p", identifier, filesystem);
	table[identifier] = filesystem;
	return identifier;
}

void kfstable_remove(kfsid_t identifier) {
	klog("kfstable_remove %lli (kfsid_t)", identifier);
	if (table[identifier]) {
		table[identifier] = NULL;
	}
}

const kfsfilesystem_t *kfstable_get(kfsid_t identifier) {
	klog("kfstable_get %lld (kfsid_t)", identifier);
	const kfsfilesystem_t *result = table[identifier];
	if(!result) {
		klog("No filesystem for identifier %lld (kfsid_t)", identifier);
	}
    klog("Return %p (kfsfilesystem_t) for %lld (kfsid_t)", result, identifier);
	return result;
}
