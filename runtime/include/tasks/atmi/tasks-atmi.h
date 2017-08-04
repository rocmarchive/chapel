/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _tasks_qthreads_h_
#define _tasks_qthreads_h_

#include "chpl-atmi.h"
#include "chpl-tasks-prvdata.h"
#include "chpltypes.h"
#include "qthread.h"
#include "qthread-chapel.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHPL_COMM_YIELD_TASK_WHILE_POLLING
void chpl_task_yield(void);

//
// Type (and default value) used to communicate task identifiers
// between C code and Chapel code in the runtime.
//
typedef uint64_t chpl_taskID_t;
#define chpl_nullTaskID ATMI_TASK_HANDLE(0xFFFFFFFFFFFFFFFF) 
#ifndef CHPL_TASK_ID_STRING_MAX_LEN
#define CHPL_TASK_ID_STRING_MAX_LEN 21
#endif

//
// Sync variables
//
typedef struct {
    aligned_t lockers_in;
    aligned_t lockers_out;
    uint_fast32_t uncontested_locks;
    int       is_full;
    syncvar_t signal_full;
    syncvar_t signal_empty;
} chpl_sync_aux_t;

//
// Task private data
//

extern
#ifdef __cplusplus
"C"
#endif

volatile int chpl_qthread_done_initializing;

//
// Task layer private area argument bundle header
//
typedef struct {
  chpl_bool serial_state;
  chpl_bool countRunning;
  chpl_bool is_executeOn;
  int lineno;
  int filename;
  c_sublocid_t requestedSubloc;
  chpl_fn_int_t requested_fid;
  chpl_fn_p requested_fn;
  chpl_fn_name requested_fn_name;
  chpl_taskID_t id;
} chpl_task_bundle_t;

// Structure of task-local storage
typedef struct chpl_atmi_tls_s {
  chpl_task_bundle_t *bundle;
  // The below fields could move to chpl_task_bundleData_t
  // That would reduce the size of the task local storage,
  // but increase the size of executeOn bundles.
  chpl_task_prvData_t prvdata;
  /* Reports */
  int     lock_filename;
  int     lock_lineno;
} chpl_atmi_tls_t;

extern pthread_key_t tls_cache;

extern pthread_t chpl_qthread_process_pthread;
extern pthread_t chpl_qthread_comm_pthread;

extern chpl_atmi_tls_t chpl_qthread_process_tls;
extern chpl_atmi_tls_t chpl_qthread_comm_task_tls;

#define CHPL_TASK_STD_MODULES_INITIALIZED chpl_task_stdModulesInitialized
void chpl_task_stdModulesInitialized(void);

extern pthread_t null_thread;

// Wrap qthread_get_tasklocal() and assert that it is always available.
extern chpl_atmi_tls_t* chpl_atmi_get_tasklocal(void);

#ifdef CHPL_TASK_GET_PRVDATA_IMPL_DECL
#error "CHPL_TASK_GET_PRVDATA_IMPL_DECL is already defined!"
#else
#define CHPL_TASK_GET_PRVDATA_IMPL_DECL 1
#endif
static inline chpl_task_prvData_t* chpl_task_getPrvData(void)
{
    chpl_atmi_tls_t * data = chpl_atmi_get_tasklocal();
    if (data) {
        return &data->prvdata;
    }
    assert(data);
    return NULL;
}

//
// Sublocale support
//
#ifdef CHPL_TASK_GETSUBLOC_IMPL_DECL
#error "CHPL_TASK_GETSUBLOC_IMPL_DECL is already defined!"
#else
#define CHPL_TASK_GETSUBLOC_IMPL_DECL 1
#endif
static inline
c_sublocid_t chpl_task_getSubloc(void)
{
    chpl_atmi_tls_t * data = chpl_atmi_get_tasklocal();
    if (data) 
        return data->bundle->requestedSubloc;
    else 
        return c_sublocid_any;
}

#ifdef CHPL_TASK_SETSUBLOC_IMPL_DECL
#error "CHPL_TASK_SETSUBLOC_IMPL_DECL is already defined!"
#else
#define CHPL_TASK_SETSUBLOC_IMPL_DECL 1
#endif
static inline
void chpl_task_setSubloc(c_sublocid_t subloc)
{
    assert(subloc != c_sublocid_none);

    // Only change sublocales if the caller asked for a particular one,
    // which is not the current one, and we're a (movable) task.
    //
    // Note: It's likely that this won't work in all cases where we need
    //       it.  In particular, we envision needing to move execution
    //       from sublocale to sublocale while initializing the memory
    //       layer, in order to get the NUMA domain affinity right for
    //       the subparts of the heap.  But this will be happening well
    //       before tasking init and in any case would be done from the
    //       main thread of execution, which doesn't have a shepherd.
    //       The code below wouldn't work in that situation.
    chpl_atmi_tls_t * data = chpl_atmi_get_tasklocal();
    if (data) {
        data->bundle->requestedSubloc = subloc;
        printf("Setting ATMI requested subloc to %d\n", subloc);
    }
}

#ifdef CHPL_TASK_GETREQUESTEDSUBLOC_IMPL_DECL
#error "CHPL_TASK_GETREQUESTEDSUBLOC_IMPL_DECL is already defined!"
#else
#define CHPL_TASK_GETREQUESTEDSUBLOC_IMPL_DECL 1
#endif
static inline
c_sublocid_t chpl_task_getRequestedSubloc(void)
{
    chpl_atmi_tls_t * data = chpl_atmi_get_tasklocal();
    if (data && data->bundle) {
        return data->bundle->requestedSubloc;
    }
    
    return c_sublocid_any;
}

//
// Can we support remote caching?
//
#ifdef CHPL_TASK_SUPPORTS_REMOTE_CACHE_IMPL_DECL
#error "CHPL_TASK_SUPPORTS_REMOTE_CACHE_IMPL_DECL is already defined!"
#else
#define CHPL_TASK_SUPPORTS_REMOTE_CACHE_IMPL_DECL 1
#endif
static inline
int chpl_task_supportsRemoteCache(void) {
  return CHPL_QTHREAD_SUPPORTS_REMOTE_CACHE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

#endif // ifndef _tasks_qthreads_h_
/* vim:set expandtab: */