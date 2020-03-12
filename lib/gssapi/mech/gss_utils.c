/*-
 * Copyright (c) 2005 Doug Rabson
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
 *	$FreeBSD: src/lib/libgssapi/gss_utils.c,v 1.1 2005/12/29 14:40:20 dfr Exp $
 */

#include "mech_locl.h"

static OM_uint32
_gss_copy_oid(OM_uint32 *minor_status,
	      gss_const_OID from_oid,
	      gss_OID to_oid)
{
	size_t len = from_oid->length;

	*minor_status = 0;
	to_oid->elements = malloc(len);
	if (!to_oid->elements) {
		to_oid->length = 0;
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	}
	to_oid->length = (OM_uint32)len;
	memcpy(to_oid->elements, from_oid->elements, len);
	return (GSS_S_COMPLETE);
}

OM_uint32
_gss_free_oid(OM_uint32 *minor_status, gss_OID oid)
{
	*minor_status = 0;
	if (oid->elements) {
	    free(oid->elements);
	    oid->elements = NULL;
	    oid->length = 0;
	}
	return (GSS_S_COMPLETE);
}

struct _gss_interned_oid {
    HEIM_SLIST_ENTRY(_gss_interned_oid) gio_link;
    gss_OID_desc gio_oid;
};

static HEIM_SLIST_HEAD(_gss_interned_oid_list, _gss_interned_oid) interned_oids =
HEIM_SLIST_HEAD_INITIALIZER(interned_oids);

extern gss_OID _gss_ot_internal[];
extern size_t _gss_ot_internal_count;

static OM_uint32
intern_oid_static(OM_uint32 *minor_status,
		  gss_const_OID from_oid,
		  gss_OID *to_oid)
{
    size_t i;

    /* statically allocated OIDs */
    for (i = 0; i < _gss_ot_internal_count; i++) {
	if (gss_oid_equal(_gss_ot_internal[i], from_oid)) {
	    *minor_status = 0;
	    *to_oid = _gss_ot_internal[i];
	    return GSS_S_COMPLETE;
	}
    }

    return GSS_S_CONTINUE_NEEDED;
}

OM_uint32
_gss_intern_oid(OM_uint32 *minor_status,
		gss_const_OID from_oid,
		gss_OID *to_oid)
{
    OM_uint32 major_status;
    struct _gss_interned_oid *iop;

    major_status = intern_oid_static(minor_status, from_oid, to_oid);
    if (major_status != GSS_S_CONTINUE_NEEDED)
	return major_status;

    HEIM_SLIST_ATOMIC_FOREACH(iop, &interned_oids, gio_link) {
	if (gss_oid_equal(&iop->gio_oid, from_oid)) {
	    *minor_status = 0;
	    *to_oid = &iop->gio_oid;
	    return GSS_S_COMPLETE;
	}
    }

    iop = malloc(sizeof(*iop));
    if (iop == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    major_status = _gss_copy_oid(minor_status, from_oid, &iop->gio_oid);
    if (GSS_ERROR(major_status)) {
	free(iop);
	return major_status;
    }

    HEIM_SLIST_ATOMIC_INSERT_HEAD(&interned_oids, iop, gio_link);

    *minor_status = 0;
    *to_oid = &iop->gio_oid;

    return GSS_S_COMPLETE;
}

OM_uint32
_gss_copy_buffer(OM_uint32 *minor_status,
    const gss_buffer_t from_buf, gss_buffer_t to_buf)
{
	size_t len = from_buf->length;

	*minor_status = 0;
	to_buf->value = malloc(len);
	if (!to_buf->value) {
		*minor_status = ENOMEM;
		to_buf->length = 0;
		return GSS_S_FAILURE;
	}
	to_buf->length = len;
	memcpy(to_buf->value, from_buf->value, len);
	return (GSS_S_COMPLETE);
}

void
_gss_mg_encode_le_uint32(uint32_t n, uint8_t *p)
{
    p[0] = (n >> 0 ) & 0xFF;
    p[1] = (n >> 8 ) & 0xFF;
    p[2] = (n >> 16) & 0xFF;
    p[3] = (n >> 24) & 0xFF;
}

void
_gss_mg_decode_le_uint32(const void *ptr, uint32_t *n)
{
    const uint8_t *p = ptr;
    *n = (p[0] << 0) | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

void
_gss_mg_encode_be_uint32(uint32_t n, uint8_t *p)
{
    p[0] = (n >> 24) & 0xFF;
    p[1] = (n >> 16) & 0xFF;
    p[2] = (n >> 8 ) & 0xFF;
    p[3] = (n >> 0 ) & 0xFF;
}

void
_gss_mg_decode_be_uint32(const void *ptr, uint32_t *n)
{
    const uint8_t *p = ptr;
    *n = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
}

void
_gss_mg_encode_le_uint16(uint16_t n, uint8_t *p)
{
    p[0] = (n >> 0 ) & 0xFF;
    p[1] = (n >> 8 ) & 0xFF;
}

void
_gss_mg_decode_le_uint16(const void *ptr, uint16_t *n)
{
    const uint8_t *p = ptr;
    *n = (p[0] << 0) | (p[1] << 8);
}

void
_gss_mg_encode_be_uint16(uint16_t n, uint8_t *p)
{
    p[0] = (n >> 8) & 0xFF;
    p[1] = (n >> 0) & 0xFF;
}

void
_gss_mg_decode_be_uint16(const void *ptr, uint16_t *n)
{
    const uint8_t *p = ptr;
    *n = (p[0] << 24) | (p[1] << 16);
}
