#include "asn1fix_internal.h"
#include "asn1fix.h"

char const *
asn1f_printable_reference(asn1p_ref_t *ref) {
	if(ref) {
		asn1p_value_t v;

		v.type = ATV_REFERENCED;
		v.value.reference = ref;

		return asn1f_printable_value(&v);
	} else {
		return "<no ref>";
	}
}

char const *
asn1f_printable_value(asn1p_value_t *v) {
	static char buf[128];
	static char *managedptr;
	static int managedptr_len;
	int ret;

#define	ENSURE(len)	do {						\
		int __len = (len);					\
		if(__len >= managedptr_len) {				\
			if(managedptr)					\
				free(managedptr);			\
			managedptr = malloc(__len + 1);			\
			if(managedptr) {				\
				managedptr_len = __len;			\
			} else {					\
				managedptr_len = 0;			\
				return "<memory allocation error>";	\
			}						\
		}							\
	} while(0)

	if(v == NULL)
		return "<no value>";

	switch(v->type) {
	case ATV_NOVALUE:
		return "<NO VALUE>";
	case ATV_NULL:
		return "NULL";
	case ATV_REAL:
		ret = snprintf(buf, sizeof(buf), "%f", v->value.v_double);
		if(ret >= (ssize_t)sizeof(buf))
			memcpy(buf + sizeof(buf) - 4, "...", 4);
		return buf;
	case ATV_INTEGER:
		ret = snprintf(buf, sizeof(buf), "%lld",
			(long long)v->value.v_integer);
		if(ret >= (ssize_t)sizeof(buf))
			memcpy(buf + sizeof(buf) - 4, "...", 4);
		return buf;
	case ATV_MIN: return "MIN";
	case ATV_MAX: return "MAX";
	case ATV_FALSE: return "FALSE";
	case ATV_TRUE: return "TRUE";
	case ATV_STRING:
	case ATV_UNPARSED:
		/* Buffer is guaranteed to be null-terminated */
		assert(v->value.string.buf[v->value.string.size] == '\0');
		return v->value.string.buf;
	case ATV_BITVECTOR:
		{
			uint8_t *bitvector;
			char *ptr;
			int len;
			int i;
			/*
			 * Compute number of bytes necessary
			 * to represend the binary value.
			 */
			int bits = v->value.binary_vector.size_in_bits;
			len = ((bits%8)?bits:(bits >> 2)) + sizeof("''H");
			/*
			 * Reallocate managed buffer
			 */
			ENSURE(len);

			/*
			 * Fill the buffer.
			 */
			ptr = managedptr;
			bitvector = v->value.binary_vector.bits;
			*ptr++ = '\'';
			if(bits%8) {
				/*
				 * Dump bit by bit.
				 */
				for(i = 0; i < bits; i++) {
					uint8_t uc;
					uc = bitvector[i>>3];
					*ptr++ = ((uc >> (7-(i%8)))&1)?'1':'0';
				}
			} else {
				static const char *hextable="0123456789ABCDEF";
				/*
				 * Dump byte by byte.
				 */
				for(i = 0; i < (bits >> 3); i++) {
					*ptr++ = hextable[bitvector[i] >> 4];
					*ptr++ = hextable[bitvector[i] & 0x0f];
				}
			}
			*ptr++ = '\'';
			*ptr++ = (bits%8)?'B':'H';
			*ptr++ = 'H';
			assert((ptr - managedptr) == len);
			return managedptr;
		}
	case ATV_REFERENCED:
		{
			asn1p_ref_t *ref;
			char reflen;
			char *ptr;
			int i;

			assert(v->value.reference);
			ref = v->value.reference;
			reflen = ref->comp_count;	/* Number of dots */
			for(i = 0; i < ref->comp_count; i++)
				reflen += strlen(ref->components[i].name);
			/*
			 * Make sure we have a buffer of this size.
			 */
			ENSURE(reflen);

			/*
			 * Fill-up the buffer.
			 */
			ptr = managedptr;
			for(i = 0; i < ref->comp_count; i++) {
				char *nc;
				if(i) *ptr++ = '.';
				for(nc = ref->components[i].name; *nc; nc++)
					*ptr++ = *nc;
			}
			*ptr++ = '\0';
			assert(reflen == (ptr - managedptr));
			return managedptr;
		}
	case ATV_CHOICE_IDENTIFIER:
		{
			char *cid = v->value.choice_identifier.identifier;
			char const *vptr = asn1f_printable_value(
					v->value.choice_identifier.value);
			char *val;

			val = strdup(vptr);
			if(!val) return "<memory allocation error>";

			ENSURE(strlen(cid) + sizeof(": ") + strlen(val));

			ret = snprintf(managedptr, managedptr_len + 1,
				"%s: %s", cid, val);
			assert(ret >= 0 && ret <= managedptr_len);
			free(val);
			return managedptr;
		}
	}

	return "<some complex value>";
}


/*
 * Recursively invoke a given function over the given expr and all its
 * children.
 */
int
asn1f_recurse_expr(arg_t *arg, int (*callback)(arg_t *arg)) {
	asn1p_expr_t *expr = arg->expr;
	int rvalue = 0;
	int ret;

	assert(expr);

	/*
	 * Invoke the callback at this very level.
	 */
	ret = callback(arg);
	RET2RVAL(ret, rvalue);

	/*
	 * Recursively invoke myself
	 * to iterate over each element in the tree.
	 */
	TQ_FOR(arg->expr, &(expr->members), next) {
		assert(arg->expr->expr_type != A1TC_INVALID);
		assert(arg->expr->parent_expr == expr);
		ret = asn1f_recurse_expr(arg, callback);
		RET2RVAL(ret, rvalue);
	}

	arg->expr = expr;	/* Restore original position */

	return rvalue;
}


/*
 * Check that every child of a given expr has unique name or does not have any.
 */
int
asn1f_check_unique_expr(arg_t *arg,
		int (*opt_compare)(asn1p_expr_t *a, asn1p_expr_t *b)) {
	asn1p_expr_t *expr;
	int rvalue = 0;

	TQ_FOR(expr, &(arg->expr->members), next) {
		if(expr->Identifier) {
			int ret = asn1f_check_unique_expr_child(arg, expr,
				opt_compare);
			if(ret) rvalue = -1;
		} else {
			/*
			 * No point of comparing this child with any other:
			 * this one does not have a name.
			 */
		}
	}

	return rvalue;
}

/*
 * Check that every preceeding child of the given expr is not
 * having the name of the given one.
 */
int
asn1f_check_unique_expr_child(arg_t *arg, asn1p_expr_t *child,
		int (*opt_compare)(asn1p_expr_t *a, asn1p_expr_t *b)) {
	asn1p_expr_t *expr;
	int rvalue = 0;

	assert(child);
	assert(opt_compare || child->Identifier);

	TQ_FOR(expr, &(arg->expr->members), next) {
		int ret;

		if(expr == child)
			break;

		/*
		 * Compare according to the custom rule or default
		 * names comparisons.
		 */
		if(opt_compare) {
			ret = opt_compare(expr, child);
		} else {
			if(expr->Identifier == NULL
			|| expr->expr_type == A1TC_EXTENSIBLE)
				continue;
			ret = strcasecmp(expr->Identifier, child->Identifier);
		}

		if(ret == 0) {
			char *msg;
			msg = opt_compare
				?"Expressions clash"
				:"Identifiers name clash";
			arg->eh(1,
				"%s: "
				"\"%s\" at line %d has similar %s with "
				"\"%s\" at line %d",
				msg,
				expr->Identifier,
				expr->_lineno,
				opt_compare?"property":"name",
				child->Identifier,
				child->_lineno
			);

			rvalue = -1;
		}
	}

	return rvalue;
}

int
asn1f_count_children(asn1p_expr_t *expr) {
	asn1p_expr_t *child;
	int count = 0;

	TQ_FOR(child, &(expr->members), next) {
		count++;
	}

	return count;
}


static char **known_types;
static int known_types_count;
static int known_types_size;

static int _known_types_cmp(const void *ap, const void *bp) {
	const char *a = *(const char * const *)ap;
	const char *b = *(const char * const *)bp;
	return strcmp(a, b);
}

int
asn1f_make_known_external_type(const char *type_name) {
	char *tname;

	/* Check for duplicates */
	if(asn1f_check_known_external_type(type_name) == 0) {
		errno = EEXIST;
		return -1;
	}

	/* Ensure enough space */
	if(known_types_count <= known_types_size) {
		int n = known_types_size ? known_types_size << 1 : 4;
		void *p;
		p = realloc(known_types, n * sizeof(known_types[0]));
		if(!p) return -1;
		known_types = p;
		known_types_size = n;
	}

	tname = strdup(type_name);
	if(!tname) return -1;

	known_types[known_types_count++] = tname;

#ifdef	HAVE_MERGESORT
	mergesort
#else
	qsort
#endif
	(known_types, known_types_count, sizeof(known_types[0]),
		_known_types_cmp);

	return 0;
}

int
asn1f_check_known_external_type(const char *type_name) {
	void *p = bsearch(&type_name, known_types, known_types_count,
		sizeof(known_types[0]), _known_types_cmp);
	if(p) return 0;
	errno = ESRCH;
	return -1;
}

