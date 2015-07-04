/*
 * Copyright © 2007 Luca Barbato
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Luca Barbato not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Luca Barbato makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Luca Barbato (lu_zero@gentoo.org)
 *
 * Based on fbmmx.c by Owen Taylor, Søren Sandmann and Nicholas Miell
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "pixman-private.h"
#include "pixman-combine32.h"
#include <altivec.h>

#define AVV(x...) {x}

static force_inline vector unsigned int
splat_alpha (vector unsigned int pix)
{
#ifdef WORDS_BIGENDIAN
    return vec_perm (pix, pix,
		     (vector unsigned char)AVV (
			 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
			 0x08, 0x08, 0x08, 0x08, 0x0C, 0x0C, 0x0C, 0x0C));
#else
    return vec_perm (pix, pix,
		     (vector unsigned char)AVV (
			 0x03, 0x03, 0x03, 0x03, 0x07, 0x07, 0x07, 0x07,
			 0x0B, 0x0B, 0x0B, 0x0B, 0x0F, 0x0F, 0x0F, 0x0F));
#endif
}

static force_inline vector unsigned int
pix_multiply (vector unsigned int p, vector unsigned int a)
{
    vector unsigned short hi, lo, mod;

    /* unpack to short */
    hi = (vector unsigned short)
#ifdef WORDS_BIGENDIAN
	vec_mergeh ((vector unsigned char)AVV (0),
		    (vector unsigned char)p);
#else
	vec_mergeh ((vector unsigned char) p,
		    (vector unsigned char) AVV (0));
#endif

    mod = (vector unsigned short)
#ifdef WORDS_BIGENDIAN
	vec_mergeh ((vector unsigned char)AVV (0),
		    (vector unsigned char)a);
#else
	vec_mergeh ((vector unsigned char) a,
		    (vector unsigned char) AVV (0));
#endif

    hi = vec_mladd (hi, mod, (vector unsigned short)
                    AVV (0x0080, 0x0080, 0x0080, 0x0080,
                         0x0080, 0x0080, 0x0080, 0x0080));

    hi = vec_adds (hi, vec_sr (hi, vec_splat_u16 (8)));

    hi = vec_sr (hi, vec_splat_u16 (8));

    /* unpack to short */
    lo = (vector unsigned short)
#ifdef WORDS_BIGENDIAN
	vec_mergel ((vector unsigned char)AVV (0),
		    (vector unsigned char)p);
#else
	vec_mergel ((vector unsigned char) p,
		    (vector unsigned char) AVV (0));
#endif

    mod = (vector unsigned short)
#ifdef WORDS_BIGENDIAN
	vec_mergel ((vector unsigned char)AVV (0),
		    (vector unsigned char)a);
#else
	vec_mergel ((vector unsigned char) a,
		    (vector unsigned char) AVV (0));
#endif

    lo = vec_mladd (lo, mod, (vector unsigned short)
                    AVV (0x0080, 0x0080, 0x0080, 0x0080,
                         0x0080, 0x0080, 0x0080, 0x0080));

    lo = vec_adds (lo, vec_sr (lo, vec_splat_u16 (8)));

    lo = vec_sr (lo, vec_splat_u16 (8));

    return (vector unsigned int)vec_packsu (hi, lo);
}

static force_inline vector unsigned int
pix_add (vector unsigned int a, vector unsigned int b)
{
    return (vector unsigned int)vec_adds ((vector unsigned char)a,
                                          (vector unsigned char)b);
}

static force_inline vector unsigned int
pix_add_mul (vector unsigned int x,
             vector unsigned int a,
             vector unsigned int y,
             vector unsigned int b)
{
    vector unsigned int t1, t2;

    t1 = pix_multiply (x, a);
    t2 = pix_multiply (y, b);

    return pix_add (t1, t2);
}

static force_inline vector unsigned int
negate (vector unsigned int src)
{
    return vec_nor (src, src);
}

/* dest*~srca + src */
static force_inline vector unsigned int
over (vector unsigned int src,
      vector unsigned int srca,
      vector unsigned int dest)
{
    vector unsigned char tmp = (vector unsigned char)
	pix_multiply (dest, negate (srca));

    tmp = vec_adds ((vector unsigned char)src, tmp);
    return (vector unsigned int)tmp;
}

/* in == pix_multiply */
#define in_over(src, srca, mask, dest)					\
    over (pix_multiply (src, mask),					\
          pix_multiply (srca, mask), dest)

#ifdef WORDS_BIGENDIAN

#define COMPUTE_SHIFT_MASK(source)					\
    source ## _mask = vec_lvsl (0, source);

#define COMPUTE_SHIFT_MASKS(dest, source)				\
    source ## _mask = vec_lvsl (0, source);

#define COMPUTE_SHIFT_MASKC(dest, source, mask)				\
    mask ## _mask = vec_lvsl (0, mask);					\
    source ## _mask = vec_lvsl (0, source);

/* notice you have to declare temp vars...
 * Note: tmp3 and tmp4 must remain untouched!
 */

#define LOAD_VECTORS(dest, source)			  \
do {							  \
    vector unsigned char tmp1, tmp2;			  \
    tmp1 = (typeof(tmp1))vec_ld (0, source);		  \
    tmp2 = (typeof(tmp2))vec_ld (15, source);		  \
    v ## source = (typeof(v ## source))			  \
	vec_perm (tmp1, tmp2, source ## _mask);		  \
    v ## dest = (typeof(v ## dest))vec_ld (0, dest);	  \
} while (0);

#define LOAD_VECTORSC(dest, source, mask)		  \
do {							  \
    vector unsigned char tmp1, tmp2;			  \
    tmp1 = (typeof(tmp1))vec_ld (0, source);		  \
    tmp2 = (typeof(tmp2))vec_ld (15, source);		  \
    v ## source = (typeof(v ## source))			  \
	vec_perm (tmp1, tmp2, source ## _mask);		  \
    tmp1 = (typeof(tmp1))vec_ld (0, mask);		  \
    v ## dest = (typeof(v ## dest))vec_ld (0, dest);	  \
    tmp2 = (typeof(tmp2))vec_ld (15, mask);		  \
    v ## mask = (typeof(v ## mask))			  \
    vec_perm (tmp1, tmp2, mask ## _mask);		  \
} while (0);

#define DECLARE_SRC_MASK_VAR vector unsigned char src_mask
#define DECLARE_MASK_MASK_VAR vector unsigned char mask_mask

#else

/* Now the COMPUTE_SHIFT_{MASK, MASKS, MASKC} below are just no-op.
 * They are defined that way because little endian altivec can do unaligned
 * reads natively and have no need for constructing the permutation pattern
 * variables.
 */
#define COMPUTE_SHIFT_MASK(source)

#define COMPUTE_SHIFT_MASKS(dest, source)

#define COMPUTE_SHIFT_MASKC(dest, source, mask)

# define LOAD_VECTORS(dest, source)			\
    v ## source = *((typeof(v ## source)*)source);	\
    v ## dest = *((typeof(v ## dest)*)dest);

# define LOAD_VECTORSC(dest, source, mask)		\
    v ## source = *((typeof(v ## source)*)source);	\
    v ## dest = *((typeof(v ## dest)*)dest);		\
    v ## mask = *((typeof(v ## mask)*)mask);

#define DECLARE_SRC_MASK_VAR
#define DECLARE_MASK_MASK_VAR

#endif /* WORDS_BIGENDIAN */

#define LOAD_VECTORSM(dest, source, mask)				\
    LOAD_VECTORSC (dest, source, mask)					\
    v ## source = pix_multiply (v ## source,				\
                                splat_alpha (v ## mask));

#define STORE_VECTOR(dest)						\
    vec_st ((vector unsigned int) v ## dest, 0, dest);

static void
vmx_combine_over_u_no_mask (uint32_t *      dest,
                            const uint32_t *src,
                            int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4 (d, ia, s);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {

	LOAD_VECTORS (dest, src);

	vdest = over (vsrc, splat_alpha (vsrc), vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4 (d, ia, s);

	dest[i] = d;
    }
}

static void
vmx_combine_over_u_mask (uint32_t *      dest,
                         const uint32_t *src,
                         const uint32_t *mask,
                         int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t ia;

	UN8x4_MUL_UN8 (s, m);

	ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4 (d, ia, s);
	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = over (vsrc, splat_alpha (vsrc), vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t ia;

	UN8x4_MUL_UN8 (s, m);

	ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4 (d, ia, s);
	dest[i] = d;
    }
}

static void
vmx_combine_over_u (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    if (mask)
	vmx_combine_over_u_mask (dest, src, mask, width);
    else
	vmx_combine_over_u_no_mask (dest, src, width);
}

static void
vmx_combine_over_reverse_u_no_mask (uint32_t *      dest,
                                    const uint32_t *src,
                                    int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8_ADD_UN8x4 (s, ia, d);
	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {

	LOAD_VECTORS (dest, src);

	vdest = over (vdest, splat_alpha (vdest), vsrc);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t ia = ALPHA_8 (~dest[i]);

	UN8x4_MUL_UN8_ADD_UN8x4 (s, ia, d);
	dest[i] = s;
    }
}

static void
vmx_combine_over_reverse_u_mask (uint32_t *      dest,
                                 const uint32_t *src,
                                 const uint32_t *mask,
                                 int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8 (s, m);

	UN8x4_MUL_UN8_ADD_UN8x4 (s, ia, d);
	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {

	LOAD_VECTORSM (dest, src, mask);

	vdest = over (vdest, splat_alpha (vdest), vsrc);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t ia = ALPHA_8 (~dest[i]);

	UN8x4_MUL_UN8 (s, m);

	UN8x4_MUL_UN8_ADD_UN8x4 (s, ia, d);
	dest[i] = s;
    }
}

static void
vmx_combine_over_reverse_u (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    if (mask)
	vmx_combine_over_reverse_u_mask (dest, src, mask, width);
    else
	vmx_combine_over_reverse_u_no_mask (dest, src, width);
}

static void
vmx_combine_in_u_no_mask (uint32_t *      dest,
                          const uint32_t *src,
                          int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t a = ALPHA_8 (*dest);

	UN8x4_MUL_UN8 (s, a);
	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_multiply (vsrc, splat_alpha (vdest));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t a = ALPHA_8 (dest[i]);

	UN8x4_MUL_UN8 (s, a);
	dest[i] = s;
    }
}

static void
vmx_combine_in_u_mask (uint32_t *      dest,
                       const uint32_t *src,
                       const uint32_t *mask,
                       int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t a = ALPHA_8 (*dest);

	UN8x4_MUL_UN8 (s, m);
	UN8x4_MUL_UN8 (s, a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_multiply (vsrc, splat_alpha (vdest));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t a = ALPHA_8 (dest[i]);

	UN8x4_MUL_UN8 (s, m);
	UN8x4_MUL_UN8 (s, a);

	dest[i] = s;
    }
}

static void
vmx_combine_in_u (pixman_implementation_t *imp,
                  pixman_op_t              op,
                  uint32_t *               dest,
                  const uint32_t *         src,
                  const uint32_t *         mask,
                  int                      width)
{
    if (mask)
	vmx_combine_in_u_mask (dest, src, mask, width);
    else
	vmx_combine_in_u_no_mask (dest, src, width);
}

static void
vmx_combine_in_reverse_u_no_mask (uint32_t *      dest,
                                  const uint32_t *src,
                                  int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t d = *dest;
	uint32_t a = ALPHA_8 (*src++);

	UN8x4_MUL_UN8 (d, a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_multiply (vdest, splat_alpha (vsrc));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t d = dest[i];
	uint32_t a = ALPHA_8 (src[i]);

	UN8x4_MUL_UN8 (d, a);

	dest[i] = d;
    }
}

static void
vmx_combine_in_reverse_u_mask (uint32_t *      dest,
                               const uint32_t *src,
                               const uint32_t *mask,
                               int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t d = *dest;
	uint32_t a = *src++;

	UN8x4_MUL_UN8 (a, m);
	a = ALPHA_8 (a);
	UN8x4_MUL_UN8 (d, a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_multiply (vdest, splat_alpha (vsrc));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t d = dest[i];
	uint32_t a = src[i];

	UN8x4_MUL_UN8 (a, m);
	a = ALPHA_8 (a);
	UN8x4_MUL_UN8 (d, a);

	dest[i] = d;
    }
}

static void
vmx_combine_in_reverse_u (pixman_implementation_t *imp,
                          pixman_op_t              op,
                          uint32_t *               dest,
                          const uint32_t *         src,
                          const uint32_t *         mask,
                          int                      width)
{
    if (mask)
	vmx_combine_in_reverse_u_mask (dest, src, mask, width);
    else
	vmx_combine_in_reverse_u_no_mask (dest, src, width);
}

static void
vmx_combine_out_u_no_mask (uint32_t *      dest,
                           const uint32_t *src,
                           int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t a = ALPHA_8 (~(*dest));

	UN8x4_MUL_UN8 (s, a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_multiply (vsrc, splat_alpha (negate (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t a = ALPHA_8 (~dest[i]);

	UN8x4_MUL_UN8 (s, a);

	dest[i] = s;
    }
}

static void
vmx_combine_out_u_mask (uint32_t *      dest,
                        const uint32_t *src,
                        const uint32_t *mask,
                        int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t a = ALPHA_8 (~(*dest));

	UN8x4_MUL_UN8 (s, m);
	UN8x4_MUL_UN8 (s, a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_multiply (vsrc, splat_alpha (negate (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t a = ALPHA_8 (~dest[i]);

	UN8x4_MUL_UN8 (s, m);
	UN8x4_MUL_UN8 (s, a);

	dest[i] = s;
    }
}

static void
vmx_combine_out_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    if (mask)
	vmx_combine_out_u_mask (dest, src, mask, width);
    else
	vmx_combine_out_u_no_mask (dest, src, width);
}

static void
vmx_combine_out_reverse_u_no_mask (uint32_t *      dest,
                                   const uint32_t *src,
                                   int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t d = *dest;
	uint32_t a = ALPHA_8 (~(*src++));

	UN8x4_MUL_UN8 (d, a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {

	LOAD_VECTORS (dest, src);

	vdest = pix_multiply (vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t d = dest[i];
	uint32_t a = ALPHA_8 (~src[i]);

	UN8x4_MUL_UN8 (d, a);

	dest[i] = d;
    }
}

static void
vmx_combine_out_reverse_u_mask (uint32_t *      dest,
                                const uint32_t *src,
                                const uint32_t *mask,
                                int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t d = *dest;
	uint32_t a = *src++;

	UN8x4_MUL_UN8 (a, m);
	a = ALPHA_8 (~a);
	UN8x4_MUL_UN8 (d, a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_multiply (vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t d = dest[i];
	uint32_t a = src[i];

	UN8x4_MUL_UN8 (a, m);
	a = ALPHA_8 (~a);
	UN8x4_MUL_UN8 (d, a);

	dest[i] = d;
    }
}

static void
vmx_combine_out_reverse_u (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           uint32_t *               dest,
                           const uint32_t *         src,
                           const uint32_t *         mask,
                           int                      width)
{
    if (mask)
	vmx_combine_out_reverse_u_mask (dest, src, mask, width);
    else
	vmx_combine_out_reverse_u_no_mask (dest, src, width);
}

static void
vmx_combine_atop_u_no_mask (uint32_t *      dest,
                            const uint32_t *src,
                            int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t dest_a = ALPHA_8 (d);
	uint32_t src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_a, d, src_ia);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_add_mul (vsrc, splat_alpha (vdest),
			     vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t dest_a = ALPHA_8 (d);
	uint32_t src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_a, d, src_ia);

	dest[i] = s;
    }
}

static void
vmx_combine_atop_u_mask (uint32_t *      dest,
                         const uint32_t *src,
                         const uint32_t *mask,
                         int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t dest_a = ALPHA_8 (d);
	uint32_t src_ia;

	UN8x4_MUL_UN8 (s, m);

	src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_a, d, src_ia);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_add_mul (vsrc, splat_alpha (vdest),
			     vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t dest_a = ALPHA_8 (d);
	uint32_t src_ia;

	UN8x4_MUL_UN8 (s, m);

	src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_a, d, src_ia);

	dest[i] = s;
    }
}

static void
vmx_combine_atop_u (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    if (mask)
	vmx_combine_atop_u_mask (dest, src, mask, width);
    else
	vmx_combine_atop_u_no_mask (dest, src, width);
}

static void
vmx_combine_atop_reverse_u_no_mask (uint32_t *      dest,
                                    const uint32_t *src,
                                    int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t src_a = ALPHA_8 (s);
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_add_mul (vdest, splat_alpha (vsrc),
			     vsrc, splat_alpha (negate (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t src_a = ALPHA_8 (s);
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_a);

	dest[i] = s;
    }
}

static void
vmx_combine_atop_reverse_u_mask (uint32_t *      dest,
                                 const uint32_t *src,
                                 const uint32_t *mask,
                                 int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t src_a;
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8 (s, m);

	src_a = ALPHA_8 (s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_add_mul (vdest, splat_alpha (vsrc),
			     vsrc, splat_alpha (negate (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t src_a;
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8 (s, m);

	src_a = ALPHA_8 (s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_a);

	dest[i] = s;
    }
}

static void
vmx_combine_atop_reverse_u (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    if (mask)
	vmx_combine_atop_reverse_u_mask (dest, src, mask, width);
    else
	vmx_combine_atop_reverse_u_no_mask (dest, src, width);
}

static void
vmx_combine_xor_u_no_mask (uint32_t *      dest,
                           const uint32_t *src,
                           int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t src_ia = ALPHA_8 (~s);
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_ia);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_add_mul (vsrc, splat_alpha (negate (vdest)),
			     vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t src_ia = ALPHA_8 (~s);
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_ia);

	dest[i] = s;
    }
}

static void
vmx_combine_xor_u_mask (uint32_t *      dest,
                        const uint32_t *src,
                        const uint32_t *mask,
                        int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t src_ia;
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8 (s, m);

	src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_ia);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_add_mul (vsrc, splat_alpha (negate (vdest)),
			     vdest, splat_alpha (negate (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t src_ia;
	uint32_t dest_ia = ALPHA_8 (~d);

	UN8x4_MUL_UN8 (s, m);

	src_ia = ALPHA_8 (~s);

	UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8 (s, dest_ia, d, src_ia);

	dest[i] = s;
    }
}

static void
vmx_combine_xor_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    if (mask)
	vmx_combine_xor_u_mask (dest, src, mask, width);
    else
	vmx_combine_xor_u_no_mask (dest, src, width);
}

static void
vmx_combine_add_u_no_mask (uint32_t *      dest,
                           const uint32_t *src,
                           int             width)
{
    int i;
    vector unsigned int vdest, vsrc;
    DECLARE_SRC_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t s = *src++;
	uint32_t d = *dest;

	UN8x4_ADD_UN8x4 (d, s);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKS (dest, src);
    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORS (dest, src);

	vdest = pix_add (vsrc, vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t s = src[i];
	uint32_t d = dest[i];

	UN8x4_ADD_UN8x4 (d, s);

	dest[i] = d;
    }
}

static void
vmx_combine_add_u_mask (uint32_t *      dest,
                        const uint32_t *src,
                        const uint32_t *mask,
                        int             width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t m = ALPHA_8 (*mask++);
	uint32_t s = *src++;
	uint32_t d = *dest;

	UN8x4_MUL_UN8 (s, m);
	UN8x4_ADD_UN8x4 (d, s);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSM (dest, src, mask);

	vdest = pix_add (vsrc, vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t m = ALPHA_8 (mask[i]);
	uint32_t s = src[i];
	uint32_t d = dest[i];

	UN8x4_MUL_UN8 (s, m);
	UN8x4_ADD_UN8x4 (d, s);

	dest[i] = d;
    }
}

static void
vmx_combine_add_u (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    if (mask)
	vmx_combine_add_u_mask (dest, src, mask, width);
    else
	vmx_combine_add_u_no_mask (dest, src, width);
}

static void
vmx_combine_src_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;

	UN8x4_MUL_UN8x4 (s, a);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_multiply (vsrc, vmask);

	STORE_VECTOR (dest);

	mask += 4;
	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];

	UN8x4_MUL_UN8x4 (s, a);

	dest[i] = s;
    }
}

static void
vmx_combine_over_ca (pixman_implementation_t *imp,
                     pixman_op_t              op,
                     uint32_t *               dest,
                     const uint32_t *         src,
                     const uint32_t *         mask,
                     int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (s);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ~a, s);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = in_over (vsrc, splat_alpha (vsrc), vmask, vdest);

	STORE_VECTOR (dest);

	mask += 4;
	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (s);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ~a, s);

	dest[i] = d;
    }
}

static void
vmx_combine_over_reverse_ca (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             uint32_t *               dest,
                             const uint32_t *         src,
                             const uint32_t *         mask,
                             int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t ida = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8_ADD_UN8x4 (s, ida, d);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = over (vdest, splat_alpha (vdest), pix_multiply (vsrc, vmask));

	STORE_VECTOR (dest);

	mask += 4;
	src += 4;
	dest += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t ida = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8_ADD_UN8x4 (s, ida, d);

	dest[i] = s;
    }
}

static void
vmx_combine_in_ca (pixman_implementation_t *imp,
                   pixman_op_t              op,
                   uint32_t *               dest,
                   const uint32_t *         src,
                   const uint32_t *         mask,
                   int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t da = ALPHA_8 (*dest);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (s, da);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_multiply (pix_multiply (vsrc, vmask), splat_alpha (vdest));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t da = ALPHA_8 (dest[i]);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (s, da);

	dest[i] = s;
    }
}

static void
vmx_combine_in_reverse_ca (pixman_implementation_t *imp,
                           pixman_op_t              op,
                           uint32_t *               dest,
                           const uint32_t *         src,
                           const uint32_t *         mask,
                           int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (*src++);

	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4 (d, a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {

	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_multiply (vdest, pix_multiply (vmask, splat_alpha (vsrc)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (src[i]);

	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4 (d, a);

	dest[i] = d;
    }
}

static void
vmx_combine_out_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (s, da);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_multiply (
	    pix_multiply (vsrc, vmask), splat_alpha (negate (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (s, da);

	dest[i] = s;
    }
}

static void
vmx_combine_out_reverse_ca (pixman_implementation_t *imp,
                            pixman_op_t              op,
                            uint32_t *               dest,
                            const uint32_t *         src,
                            const uint32_t *         mask,
                            int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (s);

	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4 (d, ~a);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_multiply (
	    vdest, negate (pix_multiply (vmask, splat_alpha (vsrc))));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (s);

	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4 (d, ~a);

	dest[i] = d;
    }
}

static void
vmx_combine_atop_ca (pixman_implementation_t *imp,
                     pixman_op_t              op,
                     uint32_t *               dest,
                     const uint32_t *         src,
                     const uint32_t *         mask,
                     int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask, vsrca;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, ~a, s, da);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vsrca = splat_alpha (vsrc);

	vsrc = pix_multiply (vsrc, vmask);
	vmask = pix_multiply (vmask, vsrca);

	vdest = pix_add_mul (vsrc, splat_alpha (vdest),
			     negate (vmask), vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, ~a, s, da);

	dest[i] = d;
    }
}

static void
vmx_combine_atop_reverse_ca (pixman_implementation_t *imp,
                             pixman_op_t              op,
                             uint32_t *               dest,
                             const uint32_t *         src,
                             const uint32_t *         mask,
                             int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, a, s, da);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_add_mul (vdest,
			     pix_multiply (vmask, splat_alpha (vsrc)),
			     pix_multiply (vsrc, vmask),
			     negate (splat_alpha (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, a, s, da);

	dest[i] = d;
    }
}

static void
vmx_combine_xor_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, ~a, s, da);

	*dest++ = d;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_add_mul (vdest,
			     negate (pix_multiply (vmask, splat_alpha (vsrc))),
			     pix_multiply (vsrc, vmask),
			     negate (splat_alpha (vdest)));

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];
	uint32_t sa = ALPHA_8 (s);
	uint32_t da = ALPHA_8 (~d);

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_MUL_UN8 (a, sa);
	UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8 (d, ~a, s, da);

	dest[i] = d;
    }
}

static void
vmx_combine_add_ca (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    uint32_t *               dest,
                    const uint32_t *         src,
                    const uint32_t *         mask,
                    int                      width)
{
    int i;
    vector unsigned int vdest, vsrc, vmask;
    DECLARE_SRC_MASK_VAR;
    DECLARE_MASK_MASK_VAR;

    while (width && ((uintptr_t)dest & 15))
    {
	uint32_t a = *mask++;
	uint32_t s = *src++;
	uint32_t d = *dest;

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_ADD_UN8x4 (s, d);

	*dest++ = s;
	width--;
    }

    COMPUTE_SHIFT_MASKC (dest, src, mask);

    /* printf ("%s\n",__PRETTY_FUNCTION__); */
    for (i = width / 4; i > 0; i--)
    {
	LOAD_VECTORSC (dest, src, mask);

	vdest = pix_add (pix_multiply (vsrc, vmask), vdest);

	STORE_VECTOR (dest);

	src += 4;
	dest += 4;
	mask += 4;
    }

    for (i = width % 4; --i >= 0;)
    {
	uint32_t a = mask[i];
	uint32_t s = src[i];
	uint32_t d = dest[i];

	UN8x4_MUL_UN8x4 (s, a);
	UN8x4_ADD_UN8x4 (s, d);

	dest[i] = s;
    }
}

static const pixman_fast_path_t vmx_fast_paths[] =
{
    {   PIXMAN_OP_NONE	},
};

pixman_implementation_t *
_pixman_implementation_create_vmx (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp = _pixman_implementation_create (fallback, vmx_fast_paths);

    /* Set up function pointers */

    imp->combine_32[PIXMAN_OP_OVER] = vmx_combine_over_u;
    imp->combine_32[PIXMAN_OP_OVER_REVERSE] = vmx_combine_over_reverse_u;
    imp->combine_32[PIXMAN_OP_IN] = vmx_combine_in_u;
    imp->combine_32[PIXMAN_OP_IN_REVERSE] = vmx_combine_in_reverse_u;
    imp->combine_32[PIXMAN_OP_OUT] = vmx_combine_out_u;
    imp->combine_32[PIXMAN_OP_OUT_REVERSE] = vmx_combine_out_reverse_u;
    imp->combine_32[PIXMAN_OP_ATOP] = vmx_combine_atop_u;
    imp->combine_32[PIXMAN_OP_ATOP_REVERSE] = vmx_combine_atop_reverse_u;
    imp->combine_32[PIXMAN_OP_XOR] = vmx_combine_xor_u;

    imp->combine_32[PIXMAN_OP_ADD] = vmx_combine_add_u;

    imp->combine_32_ca[PIXMAN_OP_SRC] = vmx_combine_src_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER] = vmx_combine_over_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER_REVERSE] = vmx_combine_over_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_IN] = vmx_combine_in_ca;
    imp->combine_32_ca[PIXMAN_OP_IN_REVERSE] = vmx_combine_in_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT] = vmx_combine_out_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT_REVERSE] = vmx_combine_out_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP] = vmx_combine_atop_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP_REVERSE] = vmx_combine_atop_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_XOR] = vmx_combine_xor_ca;
    imp->combine_32_ca[PIXMAN_OP_ADD] = vmx_combine_add_ca;

    return imp;
}
