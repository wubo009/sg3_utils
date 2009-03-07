/*
 * Copyright (c) 2005-2009 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* sg_pt_linux version 1.11 20090307 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sg_pt.h"
#include "sg_lib.h"
#include "sg_linux_inc.h"

#define DEF_TIMEOUT 60000       /* 60,000 millisecs (60 seconds) */

static const char * linux_host_bytes[] = {
    "DID_OK", "DID_NO_CONNECT", "DID_BUS_BUSY", "DID_TIME_OUT",
    "DID_BAD_TARGET", "DID_ABORT", "DID_PARITY", "DID_ERROR",
    "DID_RESET", "DID_BAD_INTR", "DID_PASSTHROUGH", "DID_SOFT_ERROR",
    "DID_IMM_RETRY", "DID_REQUEUE"
};

#define LINUX_HOST_BYTES_SZ \
        (int)(sizeof(linux_host_bytes) / sizeof(linux_host_bytes[0]))

static const char * linux_driver_bytes[] = {
    "DRIVER_OK", "DRIVER_BUSY", "DRIVER_SOFT", "DRIVER_MEDIA",
    "DRIVER_ERROR", "DRIVER_INVALID", "DRIVER_TIMEOUT", "DRIVER_HARD",
    "DRIVER_SENSE"
};

#define LINUX_DRIVER_BYTES_SZ \
    (int)(sizeof(linux_driver_bytes) / sizeof(linux_driver_bytes[0]))

static const char * linux_driver_suggests[] = {
    "SUGGEST_OK", "SUGGEST_RETRY", "SUGGEST_ABORT", "SUGGEST_REMAP",
    "SUGGEST_DIE", "UNKNOWN","UNKNOWN","UNKNOWN",
    "SUGGEST_SENSE"
};

#define LINUX_DRIVER_SUGGESTS_SZ \
    (int)(sizeof(linux_driver_suggests) / sizeof(linux_driver_suggests[0]))

/*
 * These defines are for constants that should be visible in the
 * /usr/include/scsi directory (brought in by sg_linux_inc.h).
 * Redefined and aliased here to decouple this code from
 * sg_io_linux.h
 */
#ifndef DRIVER_MASK
#define DRIVER_MASK 0x0f
#endif
#ifndef SUGGEST_MASK
#define SUGGEST_MASK 0xf0
#endif
#ifndef DRIVER_SENSE
#define DRIVER_SENSE 0x08
#endif
#define SG_LIB_DRIVER_MASK      DRIVER_MASK
#define SG_LIB_SUGGEST_MASK     SUGGEST_MASK
#define SG_LIB_DRIVER_SENSE    DRIVER_SENSE
 


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#if defined(IGNORE_LINUX_BSG) || ! defined(HAVE_LINUX_BSG_H)
/* sgv3 via SG_IO ioctl on a sg node or other node that accepts that ioctl */


struct sg_pt_linux_scsi {
    struct sg_io_hdr io_hdr;
    int in_err;
    int os_err;
};

struct sg_pt_base {
    struct sg_pt_linux_scsi impl;
};


/* Returns >= 0 if successful. If error in Unix returns negated errno. */
int
scsi_pt_open_device(const char * device_name, int read_only, int verbose)
{
    int oflags = O_NONBLOCK;

    oflags |= (read_only ? O_RDONLY : O_RDWR);
    return scsi_pt_open_flags(device_name, oflags, verbose);
}

/* Similar to scsi_pt_open_device() but takes Unix style open flags OR-ed */
/* together. The 'flags' argument is advisory and may be ignored. */
/* Returns >= 0 if successful, otherwise returns negated errno. */
int
scsi_pt_open_flags(const char * device_name, int flags, int verbose)
{
    int fd;

    if (verbose > 1) {
        if (NULL == sg_warnings_strm)
            sg_warnings_strm = stderr;
        fprintf(sg_warnings_strm, "open %s with flags=0x%x\n", device_name,
                flags);
    }
    fd = open(device_name, flags);
    if (fd < 0)
        fd = -errno;
    return fd;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int
scsi_pt_close_device(int device_fd)
{
    int res;

    res = close(device_fd);
    if (res < 0)
        res = -errno;
    return res;
}


struct sg_pt_base *
construct_scsi_pt_obj()
{
    struct sg_pt_linux_scsi * ptp;

    ptp = (struct sg_pt_linux_scsi *)
          calloc(1, sizeof(struct sg_pt_linux_scsi));
    if (ptp) {
        ptp->io_hdr.interface_id = 'S';
        ptp->io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
    return (struct sg_pt_base *)ptp;
}

void
destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp)
        free(ptp);
}

void
clear_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp) {
        memset(ptp, 0, sizeof(struct sg_pt_linux_scsi));
        ptp->io_hdr.interface_id = 'S';
        ptp->io_hdr.dxfer_direction = SG_DXFER_NONE;
    }
}

void
set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                int cdb_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.cmdp)
        ++ptp->in_err;
    ptp->io_hdr.cmdp = (unsigned char *)cdb;
    ptp->io_hdr.cmd_len = cdb_len;
}

void
set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                  int max_sense_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.sbp)
        ++ptp->in_err;
    memset(sense, 0, max_sense_len);
    ptp->io_hdr.sbp = sense;
    ptp->io_hdr.mx_sb_len = max_sense_len;
}

/* Setup for data transfer from device */
void
set_scsi_pt_data_in(struct sg_pt_base * vp, unsigned char * dxferp,
                    int dxfer_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.dxferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->io_hdr.dxferp = dxferp;
        ptp->io_hdr.dxfer_len = dxfer_len;
        ptp->io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    }
}

/* Setup for data transfer toward device */
void
set_scsi_pt_data_out(struct sg_pt_base * vp, const unsigned char * dxferp,
                     int dxfer_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.dxferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->io_hdr.dxferp = (unsigned char *)dxferp;
        ptp->io_hdr.dxfer_len = dxfer_len;
        ptp->io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    }
}

void
set_scsi_pt_packet_id(struct sg_pt_base * vp, int pack_id)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ptp->io_hdr.pack_id = pack_id;
}

void
set_scsi_pt_tag(struct sg_pt_base * vp, uint64_t tag)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ++ptp->in_err;
    tag = tag;                  /* dummy to silence compiler */
}

/* Note that task management function codes are transport specific */
void
set_scsi_pt_task_management(struct sg_pt_base * vp, int tmf_code)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ++ptp->in_err;
    tmf_code = tmf_code;        /* dummy to silence compiler */
}

void
set_scsi_pt_task_attr(struct sg_pt_base * vp, int attribute, int priority)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ++ptp->in_err;
    attribute = attribute;      /* dummy to silence compiler */
    priority = priority;        /* dummy to silence compiler */
}

/* Executes SCSI command (or at least forwards it to lower layers).
 * Clears os_err field prior to active call (whose result may set it
 * again). */
int
do_scsi_pt(struct sg_pt_base * vp, int fd, int time_secs, int verbose)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    ptp->os_err = 0;
    if (ptp->in_err) {
        if (verbose)
            fprintf(sg_warnings_strm, "Replicated or unused set_scsi_pt... "
                    "functions\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    if (NULL == ptp->io_hdr.cmdp) {
        if (verbose)
            fprintf(sg_warnings_strm, "No SCSI command (cdb) given\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    /* io_hdr.timeout is in milliseconds */
    ptp->io_hdr.timeout = ((time_secs > 0) ? (time_secs * 1000) :
                                             DEF_TIMEOUT);
    if (ptp->io_hdr.sbp && (ptp->io_hdr.sb_len_wr > 0))
        memset(ptp->io_hdr.sbp, 0, ptp->io_hdr.sb_len_wr);
    if (ioctl(fd, SG_IO, &ptp->io_hdr) < 0) {
        ptp->os_err = errno;
        if (verbose)
            fprintf(sg_warnings_strm, "ioctl(SG_IO) failed with os_err "
                    "(errno) = %d\n", ptp->os_err);
        return -ptp->os_err;
    }
    return 0;
}

int
get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    int dr_st = ptp->io_hdr.driver_status & SG_LIB_DRIVER_MASK;
    int scsi_st = ptp->io_hdr.status & 0x7e;

    if (ptp->os_err)
        return SCSI_PT_RESULT_OS_ERR;
    else if (ptp->io_hdr.host_status)
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if (dr_st && (SG_LIB_DRIVER_SENSE != dr_st))
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if ((SG_LIB_DRIVER_SENSE == dr_st) ||
             (SAM_STAT_CHECK_CONDITION == scsi_st) ||
             (SAM_STAT_COMMAND_TERMINATED == scsi_st))
        return SCSI_PT_RESULT_SENSE;
    else if (scsi_st)
        return SCSI_PT_RESULT_STATUS;
    else
        return SCSI_PT_RESULT_GOOD;
}

int
get_scsi_pt_resid(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.resid;
}

int
get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.status;
}

int
get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.sb_len_wr;
}

int
get_scsi_pt_duration_ms(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.duration;
}

int
get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return (ptp->io_hdr.host_status << 8) + ptp->io_hdr.driver_status;
}

int
get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->os_err;
}

char *
get_scsi_pt_transport_err_str(const struct sg_pt_base * vp, int max_b_len,
                              char * b)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    int ds = ptp->io_hdr.driver_status;
    int hs = ptp->io_hdr.host_status;
    int n, m;
    char * cp = b;
    int driv, sugg;
    const char * driv_cp = "invalid";
    const char * sugg_cp = "invalid";

    m = max_b_len;
    n = 0;
    if (hs) {
        if ((hs < 0) || (hs >= LINUX_HOST_BYTES_SZ))
            n = snprintf(cp, m, "Host_status=0x%02x is invalid\n", hs);
        else
            n = snprintf(cp, m, "Host_status=0x%02x [%s]\n", hs,
                         linux_host_bytes[hs]);
    }
    m -= n;
    if (m < 1) {
        b[max_b_len - 1] = '\0';
        return b;
    }
    cp += n;
    driv = ds & SG_LIB_DRIVER_MASK;
    if (driv < LINUX_DRIVER_BYTES_SZ)
        driv_cp = linux_driver_bytes[driv];
    sugg = (ds & SG_LIB_SUGGEST_MASK) >> 4;
    if (sugg < LINUX_DRIVER_SUGGESTS_SZ)
        sugg_cp = linux_driver_suggests[sugg];
    n = snprintf(cp, m, "Driver_status=0x%02x [%s, %s]\n", ds, driv_cp,
                 sugg_cp);
    m -= n;
    if (m < 1)
        b[max_b_len - 1] = '\0';
    return b;
}

char *
get_scsi_pt_os_err_str(const struct sg_pt_base * vp, int max_b_len, char * b)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    const char * cp;

    cp = safe_strerror(ptp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#else /* allow for run selection of sg v3 or v4 (via bsg) */
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


#include <linux/types.h>
#include <linux/bsg.h>

#define DEF_TIMEOUT 60000       /* 60,000 millisecs (60 seconds) */

struct sg_pt_linux_scsi {
    struct sg_io_v4 io_hdr;	/* use v4 header as it is more general */
    int in_err;
    int os_err;
    unsigned char tmf_request[4];
};

struct sg_pt_base {
    struct sg_pt_linux_scsi impl;
};

static int bsg_major_checked = 0;
static int bsg_major = 0;



static void
find_bsg_major(int verbose)
{
    const char * proc_devices = "/proc/devices";
    FILE *fp;
    char a[128];
    char b[128];
    char * cp;
    int n;

    if (NULL == (fp = fopen(proc_devices, "r"))) {
        if (NULL == sg_warnings_strm)
            sg_warnings_strm = stderr;
	if (verbose)
            fprintf(sg_warnings_strm, "fopen %s failed: %s\n", proc_devices,
		    strerror(errno));
        return;
    }
    while ((cp = fgets(b, sizeof(b), fp))) {
	if ((1 == sscanf(b, "%s", a)) &&
	    (0 == memcmp(a, "Character", 9)))
	    break;
    }
    while (cp && (cp = fgets(b, sizeof(b), fp))) {
	if (2 == sscanf(b, "%d %s", &n, a)) {
	    if (0 == strcmp("bsg", a)) {
		bsg_major = n;
	 	break;
	    }
	} else
	    break;
    }
    if ((NULL == cp) && (verbose > 3)) {
        if (NULL == sg_warnings_strm)
            sg_warnings_strm = stderr;
        fprintf(sg_warnings_strm, "found no bsg char device in %s\n",
		proc_devices);
    }
    fclose(fp);
}


/* Returns >= 0 if successful. If error in Unix returns negated errno. */
int
scsi_pt_open_device(const char * device_name, int read_only, int verbose)
{
    int oflags = O_NONBLOCK;

    oflags |= (read_only ? O_RDONLY : O_RDWR);
    return scsi_pt_open_flags(device_name, oflags, verbose);
}

/* Similar to scsi_pt_open_device() but takes Unix style open flags OR-ed */
/* together. The 'flags' argument is advisory and may be ignored. */
/* Returns >= 0 if successful, otherwise returns negated errno. */
int
scsi_pt_open_flags(const char * device_name, int flags, int verbose)
{
    int fd;

    if (! bsg_major_checked) {
	bsg_major_checked = 1;
	find_bsg_major(verbose);
    }

    if (verbose > 1) {
        if (NULL == sg_warnings_strm)
            sg_warnings_strm = stderr;
        fprintf(sg_warnings_strm, "open %s with flags=0x%x\n", device_name,
                flags);
    }
    fd = open(device_name, flags);
    if (fd < 0)
        fd = -errno;
    return fd;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int
scsi_pt_close_device(int device_fd)
{
    int res;

    res = close(device_fd);
    if (res < 0)
        res = -errno;
    return res;
}


struct sg_pt_base *
construct_scsi_pt_obj()
{
    struct sg_pt_linux_scsi * ptp;

    ptp = (struct sg_pt_linux_scsi *)
          calloc(1, sizeof(struct sg_pt_linux_scsi));
    if (ptp)
        ptp->io_hdr.guard = 'Q';
    return (struct sg_pt_base *)ptp;
}

void
destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp)
        free(ptp);
}

void
set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                int cdb_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.request)
        ++ptp->in_err;
    /* C99 has intptr_t instead of long */
    ptp->io_hdr.request = (__u64)(long)cdb;
    ptp->io_hdr.request_len = cdb_len;
}

void
set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                  int max_sense_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.response)
        ++ptp->in_err;
    memset(sense, 0, max_sense_len);
    ptp->io_hdr.response = (__u64)(long)sense;
    ptp->io_hdr.max_response_len = max_sense_len;
}

/* Setup for data transfer from device */
void
set_scsi_pt_data_in(struct sg_pt_base * vp, unsigned char * dxferp,
                    int dxfer_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.din_xferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->io_hdr.din_xferp = (__u64)(long)dxferp;
        ptp->io_hdr.din_xfer_len = dxfer_len;
    }
}

/* Setup for data transfer toward device */
void
set_scsi_pt_data_out(struct sg_pt_base * vp, const unsigned char * dxferp,
                     int dxfer_len)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    if (ptp->io_hdr.dout_xferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->io_hdr.dout_xferp = (__u64)(long)dxferp;
        ptp->io_hdr.dout_xfer_len = dxfer_len;
    }
}

void
set_scsi_pt_packet_id(struct sg_pt_base * vp, int pack_id)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ptp->io_hdr.spare_in = pack_id;
}

void
set_scsi_pt_tag(struct sg_pt_base * vp, uint64_t tag)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ptp->io_hdr.request_tag = tag;
}

/* Note that task management function codes are transport specific */
void
set_scsi_pt_task_management(struct sg_pt_base * vp, int tmf_code)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ptp->io_hdr.subprotocol = 1;	/* SCSI task management function */
    ptp->tmf_request[0] = (unsigned char)tmf_code;	/* assume it fits */
    ptp->io_hdr.request = (__u64)(long)(&(ptp->tmf_request[0]));
    ptp->io_hdr.request_len = 1;
}

void
set_scsi_pt_task_attr(struct sg_pt_base * vp, int attribute, int priority)
{
    struct sg_pt_linux_scsi * ptp = &vp->impl;

    ptp->io_hdr.request_attr = attribute;
    ptp->io_hdr.request_priority = priority;
}

/* N.B. Returns din_resid and ignores dout_resid */
int
get_scsi_pt_resid(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.din_resid;
}

int
get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.device_status;
}

int
get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.response_len;
}

int
get_scsi_pt_duration_ms(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.duration;
}

int
get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->io_hdr.transport_status;
}

int
get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;

    return ptp->os_err;
}

/* Combine driver and transport (called "host" in linux kernel) statuses */
char *
get_scsi_pt_transport_err_str(const struct sg_pt_base * vp, int max_b_len,
                              char * b)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    int ds = ptp->io_hdr.driver_status;
    int hs = ptp->io_hdr.transport_status;
    int n, m;
    char * cp = b;
    int driv, sugg;
    const char * driv_cp = "invalid";
    const char * sugg_cp = "invalid";

    m = max_b_len;
    n = 0;
    if (hs) {
        if ((hs < 0) || (hs >= LINUX_HOST_BYTES_SZ))
            n = snprintf(cp, m, "Host_status=0x%02x is invalid\n", hs);
        else
            n = snprintf(cp, m, "Host_status=0x%02x [%s]\n", hs,
                         linux_host_bytes[hs]);
    }
    m -= n;
    if (m < 1) {
        b[max_b_len - 1] = '\0';
        return b;
    }
    cp += n;
    driv = ds & SG_LIB_DRIVER_MASK;
    if (driv < LINUX_DRIVER_BYTES_SZ)
        driv_cp = linux_driver_bytes[driv];
    sugg = (ds & SG_LIB_SUGGEST_MASK) >> 4;
    if (sugg < LINUX_DRIVER_SUGGESTS_SZ)
        sugg_cp = linux_driver_suggests[sugg];
    n = snprintf(cp, m, "Driver_status=0x%02x [%s, %s]\n", ds, driv_cp,
                 sugg_cp);
    m -= n;
    if (m < 1)
        b[max_b_len - 1] = '\0';
    return b;
}

int
get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    int dr_st = ptp->io_hdr.driver_status & SG_LIB_DRIVER_MASK;
    int scsi_st = ptp->io_hdr.device_status & 0x7e;

    if (ptp->os_err)
        return SCSI_PT_RESULT_OS_ERR;
    else if (ptp->io_hdr.transport_status)
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if (dr_st && (SG_LIB_DRIVER_SENSE != dr_st))
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if ((SG_LIB_DRIVER_SENSE == dr_st) ||
             (SAM_STAT_CHECK_CONDITION == scsi_st) ||
             (SAM_STAT_COMMAND_TERMINATED == scsi_st))
        return SCSI_PT_RESULT_SENSE;
    else if (scsi_st)
        return SCSI_PT_RESULT_STATUS;
    else
        return SCSI_PT_RESULT_GOOD;
}

char *
get_scsi_pt_os_err_str(const struct sg_pt_base * vp, int max_b_len, char * b)
{
    const struct sg_pt_linux_scsi * ptp = &vp->impl;
    const char * cp;

    cp = safe_strerror(ptp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}

#endif
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
