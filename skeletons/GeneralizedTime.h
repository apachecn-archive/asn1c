/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_GeneralizedTime_H_
#define	_GeneralizedTime_H_

#include <OCTET_STRING.h>

typedef OCTET_STRING_t GeneralizedTime_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_GeneralizedTime;

asn_struct_print_f GeneralizedTime_print;
asn_constr_check_f GeneralizedTime_constraint;
der_type_encoder_f GeneralizedTime_encode_der;
xer_type_encoder_f GeneralizedTime_encode_xer;

/***********************
 * Some handy helpers. *
 ***********************/

struct tm;	/* <time.h> */

/*
 * Convert a GeneralizedTime structure into time_t
 * and optionally into struct tm.
 * If as_gmt is given, the resulting _optional_tm4fill will have a GMT zone,
 * instead of default local one.
 * On error returns -1 and errno set to EINVAL
 */
time_t asn_GT2time(const GeneralizedTime_t *, struct tm *_optional_tm4fill,
	int as_gmt);

/* A version of the above function also returning the fractions of seconds */
time_t asn_GT2time_frac(const GeneralizedTime_t *,
	long *frac_value, long *frac_base,		/* (value/base) */
	struct tm *_optional_tm4fill, int as_gmt);

/*
 * Convert a struct tm into GeneralizedTime.
 * If _optional_gt is not given, this function will try to allocate one.
 * If force_gmt is given, the resulting GeneralizedTime will be forced
 * into a GMT time zone (encoding ends with a "Z").
 * On error, this function returns 0 and sets errno.
 */
GeneralizedTime_t *asn_time2GT(GeneralizedTime_t *_optional_gt,
	const struct tm *, int force_gmt);
GeneralizedTime_t *asn_time2GT_frac(GeneralizedTime_t *_optional_gt,
	const struct tm *, long frac_value, long frac_base, int force_gmt);

#endif	/* _GeneralizedTime_H_ */
